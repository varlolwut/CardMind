from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path


RELEASE_WORKFLOW_MARKERS = (
    "LICENSE THIRD_PARTY_NOTICES.md third_party",
    "CardMind-third-party-licenses.zip",
    "CardMind-m5stack-esp32-3.2.1-source.zip",
)


@dataclass(frozen=True)
class SourceArchive:
    url: str
    sha256: str


@dataclass(frozen=True)
class Component:
    name: str
    license_file: Path
    additional_license_files: tuple[Path, ...]
    workflow_markers: tuple[str, ...]
    source_archive: SourceArchive | None


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Verify CardMind third-party pins and license files."
    )
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--workflow", required=True, type=Path)
    parser.add_argument("--notices", required=True, type=Path)
    parser.add_argument("--project-license", required=True, type=Path)
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


def require_sha256(value: object, field: str, component: str) -> str:
    checksum = require_string(value, field, component)
    if re.fullmatch(r"[0-9a-f]{64}", checksum) is None:
        raise ValueError(
            f"{component}: field '{field}' must be 64 lowercase hex characters"
        )
    return checksum


def optional_source_archive(value: object, component: str) -> SourceArchive | None:
    if value is None:
        return None
    if not isinstance(value, dict):
        raise ValueError(f"{component}: field 'source_archive' must be an object")
    url = require_string(value.get("url"), "source_archive.url", component)
    checksum = require_sha256(
        value.get("sha256"), "source_archive.sha256", component
    )
    return SourceArchive(url, checksum)


def parse_component(value: object) -> Component:
    if not isinstance(value, dict):
        raise ValueError("Each component entry must be an object")
    name = require_string(value.get("name"), "name", "component")
    license_file = Path(require_string(value.get("license_file"), "license_file", name))
    additional = optional_string_tuple(
        value.get("additional_license_files"), "additional_license_files", name
    )
    markers = require_string_tuple(value.get("workflow_markers"), "workflow_markers", name)
    source_archive = optional_source_archive(value.get("source_archive"), name)
    require_string(value.get("version"), "version", name)
    require_string(value.get("source"), "source", name)
    require_string(value.get("license"), "license", name)
    require_string(value.get("pin"), "pin", name)
    checksum = value.get("sha256")
    if checksum is not None:
        require_sha256(checksum, "sha256", name)
        require_string(value.get("binary"), "binary", name)
    return Component(
        name,
        license_file,
        tuple(Path(path) for path in additional),
        markers,
        source_archive,
    )


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


def verify_text_file(path: Path, label: str) -> None:
    if not path.is_file():
        raise FileNotFoundError(f"{label} does not exist: {path}")
    if not path.read_text(encoding="utf-8").strip():
        raise ValueError(f"{label} is empty: {path}")


def verify_release_workflow(workflow_text: str) -> None:
    for marker in RELEASE_WORKFLOW_MARKERS:
        if marker not in workflow_text:
            raise ValueError(f"Release workflow is missing required marker '{marker}'")


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
        if component.source_archive is not None:
            if component.source_archive.url not in workflow_text:
                raise ValueError(
                    f"{component.name}: workflow is missing source archive URL"
                )
            if component.source_archive.sha256 not in workflow_text:
                raise ValueError(
                    f"{component.name}: workflow is missing source archive SHA-256"
                )


def main() -> None:
    arguments = parse_arguments()
    verify_text_file(arguments.notices, "Third-party notices")
    verify_text_file(arguments.project_license, "Project license")
    components = load_components(arguments.manifest)
    workflow_text = arguments.workflow.read_text(encoding="utf-8")
    verify_components(components, arguments.repository_root, workflow_text)
    verify_release_workflow(workflow_text)
    print(f"THIRD_PARTY result=pass components={len(components)}")


if __name__ == "__main__":
    main()
