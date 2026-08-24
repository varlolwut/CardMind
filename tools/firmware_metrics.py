from __future__ import annotations

import argparse
import json
import subprocess
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path


@dataclass(frozen=True)
class Measurements:
    app_binary_bytes: int
    app_partition_bytes: int
    app_free_bytes: int
    flash_text_bytes: int
    flash_rodata_bytes: int
    static_dram_bytes: int
    iram_bytes: int


@dataclass(frozen=True)
class Limits:
    app_binary_bytes: int
    app_free_bytes_minimum: int
    flash_text_bytes: int
    flash_rodata_bytes: int
    static_dram_bytes: int
    iram_bytes: int


@dataclass(frozen=True)
class Budget:
    warning_growth_percent: float
    limits: Limits


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create and enforce a CardMind firmware memory report."
    )
    parser.add_argument("--size-tool", required=True, type=Path)
    parser.add_argument("--elf", required=True, type=Path)
    parser.add_argument("--app-bin", required=True, type=Path)
    parser.add_argument("--partitions", required=True, type=Path)
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--budget", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    return parser.parse_args()


def require_object(value: object, field: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ValueError(f"'{field}' must be a JSON object")
    return value


def require_integer(document: dict[str, object], field: str) -> int:
    value = document.get(field)
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise ValueError(f"'{field}' must be a non-negative integer")
    return value


def require_number(document: dict[str, object], field: str) -> float:
    value = document.get(field)
    if not isinstance(value, (int, float)) or isinstance(value, bool) or value < 0:
        raise ValueError(f"'{field}' must be a non-negative number")
    return float(value)


def read_json_object(path: Path, label: str) -> dict[str, object]:
    if not path.is_file():
        raise FileNotFoundError(f"{label} does not exist: {path}")
    return require_object(json.loads(path.read_text(encoding="utf-8")), label)


def parse_size_output(output: str) -> dict[str, int]:
    sections: dict[str, int] = {}
    for raw_line in output.splitlines():
        columns = raw_line.split()
        if len(columns) < 3 or not columns[0].startswith("."):
            continue
        try:
            size = int(columns[1], 10)
        except ValueError as error:
            raise ValueError(f"Invalid size output line: {raw_line}") from error
        sections[columns[0]] = size
    if ".flash.text" not in sections or ".dram0.data" not in sections:
        raise ValueError("Size output is missing required ESP32 sections")
    return sections


def run_size_tool(size_tool: Path, elf_path: Path) -> dict[str, int]:
    if not size_tool.is_file():
        raise FileNotFoundError(f"Size tool does not exist: {size_tool}")
    if not elf_path.is_file():
        raise FileNotFoundError(f"ELF image does not exist: {elf_path}")
    result = subprocess.run(
        [str(size_tool), "-A", str(elf_path)],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"Size tool failed with exit code {result.returncode}: {result.stderr.strip()}"
        )
    return parse_size_output(result.stdout)


def parse_cardmind_partition(path: Path) -> int:
    if not path.is_file():
        raise FileNotFoundError(f"Partition table does not exist: {path}")
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        columns = tuple(column.strip() for column in line.split(","))
        if len(columns) < 5:
            raise ValueError(f"Invalid partition table line: {raw_line}")
        if columns[0] == "cardmind":
            try:
                return int(columns[4], 0)
            except ValueError as error:
                raise ValueError(f"Invalid CardMind partition size: {columns[4]}") from error
    raise ValueError("Partition table does not define a 'cardmind' partition")


def section_total(sections: dict[str, int], names: tuple[str, ...]) -> int:
    return sum(sections.get(name, 0) for name in names)


def collect_measurements(
    app_path: Path, partition_bytes: int, sections: dict[str, int]
) -> Measurements:
    if not app_path.is_file():
        raise FileNotFoundError(f"Application image does not exist: {app_path}")
    app_bytes = app_path.stat().st_size
    if app_bytes > partition_bytes:
        raise ValueError(
            f"Application image is {app_bytes} bytes and exceeds the "
            f"{partition_bytes}-byte CardMind partition"
        )
    return Measurements(
        app_binary_bytes=app_bytes,
        app_partition_bytes=partition_bytes,
        app_free_bytes=partition_bytes - app_bytes,
        flash_text_bytes=section_total(sections, (".flash.text",)),
        flash_rodata_bytes=section_total(
            sections, (".flash.appdesc", ".flash.rodata")
        ),
        static_dram_bytes=section_total(
            sections, (".dram0.data", ".dram0.bss")
        ),
        iram_bytes=section_total(
            sections, (".iram0.vectors", ".iram0.text", ".iram0.text_end")
        ),
    )


def parse_measurements(document: dict[str, object], label: str) -> Measurements:
    values = require_object(document.get("measurements"), f"{label}.measurements")
    return Measurements(
        app_binary_bytes=require_integer(values, "app_binary_bytes"),
        app_partition_bytes=require_integer(values, "app_partition_bytes"),
        app_free_bytes=require_integer(values, "app_free_bytes"),
        flash_text_bytes=require_integer(values, "flash_text_bytes"),
        flash_rodata_bytes=require_integer(values, "flash_rodata_bytes"),
        static_dram_bytes=require_integer(values, "static_dram_bytes"),
        iram_bytes=require_integer(values, "iram_bytes"),
    )


def parse_budget(document: dict[str, object]) -> Budget:
    limits = require_object(document.get("limits"), "budget.limits")
    return Budget(
        warning_growth_percent=require_number(document, "warning_growth_percent"),
        limits=Limits(
            app_binary_bytes=require_integer(limits, "app_binary_bytes"),
            app_free_bytes_minimum=require_integer(limits, "app_free_bytes_minimum"),
            flash_text_bytes=require_integer(limits, "flash_text_bytes"),
            flash_rodata_bytes=require_integer(limits, "flash_rodata_bytes"),
            static_dram_bytes=require_integer(limits, "static_dram_bytes"),
            iram_bytes=require_integer(limits, "iram_bytes"),
        ),
    )


def compare_baseline(
    current: Measurements, baseline: Measurements, growth_percent: float
) -> tuple[str, ...]:
    warnings: list[str] = []
    current_values = asdict(current)
    baseline_values = asdict(baseline)
    for field in (
        "app_binary_bytes",
        "flash_text_bytes",
        "flash_rodata_bytes",
        "static_dram_bytes",
        "iram_bytes",
    ):
        baseline_value = baseline_values[field]
        current_value = current_values[field]
        warning_value = baseline_value * (1.0 + growth_percent / 100.0)
        if current_value > warning_value:
            warnings.append(
                f"{field} grew from {baseline_value} to {current_value} bytes "
                f"(more than {growth_percent:.1f}%)"
            )
    return tuple(warnings)


def enforce_limits(current: Measurements, limits: Limits) -> tuple[str, ...]:
    failures: list[str] = []
    maximums = (
        ("app_binary_bytes", current.app_binary_bytes, limits.app_binary_bytes),
        ("flash_text_bytes", current.flash_text_bytes, limits.flash_text_bytes),
        ("flash_rodata_bytes", current.flash_rodata_bytes, limits.flash_rodata_bytes),
        ("static_dram_bytes", current.static_dram_bytes, limits.static_dram_bytes),
        ("iram_bytes", current.iram_bytes, limits.iram_bytes),
    )
    for field, current_value, maximum in maximums:
        if current_value > maximum:
            failures.append(f"{field} is {current_value} bytes; maximum is {maximum}")
    if current.app_free_bytes < limits.app_free_bytes_minimum:
        failures.append(
            f"app_free_bytes is {current.app_free_bytes}; minimum is "
            f"{limits.app_free_bytes_minimum}"
        )
    return tuple(failures)


def write_report(
    output_path: Path,
    measurements: Measurements,
    baseline: Measurements,
    budget: Budget,
    warnings: tuple[str, ...],
    failures: tuple[str, ...],
) -> None:
    report = {
        "schema_version": 1,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "status": "fail" if failures else "pass",
        "measurements": asdict(measurements),
        "baseline": asdict(baseline),
        "budget": {
            "warning_growth_percent": budget.warning_growth_percent,
            "limits": asdict(budget.limits),
        },
        "warnings": list(warnings),
        "failures": list(failures),
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def main() -> None:
    arguments = parse_arguments()
    sections = run_size_tool(arguments.size_tool, arguments.elf)
    partition_bytes = parse_cardmind_partition(arguments.partitions)
    measurements = collect_measurements(arguments.app_bin, partition_bytes, sections)
    baseline = parse_measurements(
        read_json_object(arguments.baseline, "Firmware baseline"), "baseline"
    )
    budget = parse_budget(read_json_object(arguments.budget, "Firmware budget"))
    warnings = compare_baseline(
        measurements, baseline, budget.warning_growth_percent
    )
    failures = enforce_limits(measurements, budget.limits)
    write_report(
        arguments.output, measurements, baseline, budget, warnings, failures
    )
    for warning in warnings:
        print(f"FIRMWARE_METRICS warning={warning}")
    if failures:
        raise RuntimeError("; ".join(failures))
    print(
        f"FIRMWARE_METRICS result=pass app={measurements.app_binary_bytes} "
        f"free={measurements.app_free_bytes} static_dram={measurements.static_dram_bytes} "
        f"iram={measurements.iram_bytes}"
    )


if __name__ == "__main__":
    main()
