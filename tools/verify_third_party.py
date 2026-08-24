from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Component:
    name: str
    license_file: Path
    additional_license_files: tuple[Path, ...]
    workflow_markers: tuple[str, ...]


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Verify CardMind third-party pins and license files."
    )
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--workflow", required=True, type=Path)
    parser.add_argument("--notices", required=True, type=Path)
    parser.add_argument("--repository-root", required=True, type=Path)
    return parser.parse_args()


def require_string(value: object, field: str, component: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{component}: field '{field}' must be a non-empty string")
    return value


def require_string_tuple(value: object, field: str, component: str) -> tuple[str, ...]:
    if not isinstance(value, list) or not value:
        raise ValueError(f"{component}: field '{field}' must be a non-empty array")
    result: list[str] = []
    for index, item in enumerate(value):
        result.append(require_string(item, f"{field}[{index}]", component))
    return tuple(result)


def optional_string_tuple(value: object, field: str, component: str) -> tuple[str, ...]:
    if value is None:
        return ()
    if not isinstance(value, list):
        raise ValueError(f"{component}: field '{field}' must be an array")
    result: list[str] = []
    for index, item in enumerate(value):
        result.append(require_string(item, f"{field}[{index}]", component))
    return tuple(result)


def parse_component(value: object) -> Component:
    if not isinstance(value, dict):
        raise ValueError("Each component entry must be an object")
    name = require_string(value.get("name"), "name", "component")
    license_file = Path(require_string(value.get("license_file"), "license_file", name))
    additional = optional_string_tuple(
        value.get("additional_license_files"), "additional_license_files", name
    )
    markers = require_string_tuple(value.get("workflow_markers"), "workflow_markers", name)
    require_string(value.get("version"), "version", name)
    require_string(value.get("source"), "source", name)
    require_string(value.get("license"), "license", name)
    require_string(value.get("pin"), "pin", name)
    checksum = value.get("sha256")
    if checksum is not None:
        checksum_text = require_string(checksum, "sha256", name)
        if re.fullmatch(r"[0-9a-f]{64}", checksum_text) is None:
            raise ValueError(f"{name}: field 'sha256' must be 64 lowercase hex characters")
        require_string(value.get("binary"), "binary", name)
    return Component(name, license_file, tuple(Path(path) for path in additional), markers)


def load_components(manifest_path: Path) -> tuple[Component, ...]:
    document = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not isinstance(document, dict):
        raise ValueError("Third-party manifest root must be an object")
    if document.get("schema_version") != 1:
        raise ValueError("Third-party manifest schema_version must be 1")
    raw_components = document.get("components")
    if not isinstance(raw_components, list) or not raw_components:
        raise ValueError("Third-party manifest components must be a non-empty array")
    components = tuple(parse_component(value) for value in raw_components)
    names = tuple(component.name for component in components)
    if len(names) != len(set(names)):
        raise ValueError("Third-party manifest contains duplicate component names")
    return components


def verify_license_file(repository_root: Path, relative_path: Path, component: str) -> None:
    absolute_path = repository_root / relative_path
    if not absolute_path.is_file():
        raise FileNotFoundError(f"{component}: license file does not exist: {relative_path}")
    if not absolute_path.read_text(encoding="utf-8").strip():
        raise ValueError(f"{component}: license file is empty: {relative_path}")


def verify_components(
    components: tuple[Component, ...], repository_root: Path, workflow_text: str
) -> None:
    for component in components:
        verify_license_file(repository_root, component.license_file, component.name)
        for license_file in component.additional_license_files:
            verify_license_file(repository_root, license_file, component.name)
        for marker in component.workflow_markers:
            if marker not in workflow_text:
                raise ValueError(
                    f"{component.name}: workflow is missing pinned marker '{marker}'"
                )


def main() -> None:
    arguments = parse_arguments()
    if not arguments.notices.is_file():
        raise FileNotFoundError(f"Third-party notices do not exist: {arguments.notices}")
    components = load_components(arguments.manifest)
    workflow_text = arguments.workflow.read_text(encoding="utf-8")
    verify_components(components, arguments.repository_root, workflow_text)
    print(f"THIRD_PARTY result=pass components={len(components)}")


if __name__ == "__main__":
    main()
