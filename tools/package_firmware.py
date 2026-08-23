from __future__ import annotations

import argparse
import hashlib
import shutil
from dataclasses import dataclass
from pathlib import Path


FLASH_BYTES = 0x800000
ESP_IMAGE_MAGIC = 0xE9


@dataclass(frozen=True)
class ImageRegion:
    name: str
    offset: int
    maximum_bytes: int


PYTHON_REGION = ImageRegion("MicroPython", 0x410000, 0x300000)
VFS_REGION = ImageRegion("MicroPython VFS", 0x710000, 0x20000)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Package CardMind, MicroPython and its VFS into one 8 MiB image."
    )
    parser.add_argument("--cardmind-merged", required=True, type=Path)
    parser.add_argument("--cardmind-app", required=True, type=Path)
    parser.add_argument("--micropython-app", required=True, type=Path)
    parser.add_argument("--vfs-image", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    return parser.parse_args()


def read_required_file(path: Path, label: str) -> bytes:
    if not path.is_file():
        raise FileNotFoundError(f"{label} does not exist: {path}")
    content = path.read_bytes()
    if not content:
        raise ValueError(f"{label} is empty: {path}")
    return content


def validate_app_image(content: bytes, label: str, maximum_bytes: int) -> None:
    if content[0] != ESP_IMAGE_MAGIC:
        raise ValueError(f"{label} is not an ESP application image")
    if len(content) > maximum_bytes:
        raise ValueError(
            f"{label} is {len(content)} bytes and exceeds its {maximum_bytes}-byte partition"
        )


def write_region(image: bytearray, content: bytes, region: ImageRegion) -> None:
    if len(content) > region.maximum_bytes:
        raise ValueError(
            f"{region.name} is {len(content)} bytes and exceeds its "
            f"{region.maximum_bytes}-byte region"
        )
    end = region.offset + len(content)
    if end > len(image):
        raise ValueError(f"{region.name} extends beyond the flash image")
    image[region.offset:end] = content


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def package_images(
    cardmind_merged_path: Path,
    cardmind_app_path: Path,
    micropython_app_path: Path,
    vfs_image_path: Path,
    output_dir: Path,
) -> tuple[Path, Path, Path]:
    cardmind_merged = read_required_file(cardmind_merged_path, "CardMind merged image")
    cardmind_app = read_required_file(cardmind_app_path, "CardMind app image")
    micropython_app = read_required_file(micropython_app_path, "MicroPython app image")
    vfs_image = read_required_file(vfs_image_path, "MicroPython VFS image")

    if len(cardmind_merged) != FLASH_BYTES:
        raise ValueError(
            f"CardMind merged image must be exactly {FLASH_BYTES} bytes; "
            f"received {len(cardmind_merged)}"
        )
    validate_app_image(cardmind_app, "CardMind app image", 0x400000)
    validate_app_image(micropython_app, "MicroPython app image", PYTHON_REGION.maximum_bytes)
    if len(vfs_image) != VFS_REGION.maximum_bytes:
        raise ValueError(
            f"MicroPython VFS image must be exactly {VFS_REGION.maximum_bytes} bytes; "
            f"received {len(vfs_image)}"
        )

    full_image = bytearray(cardmind_merged)
    write_region(full_image, micropython_app, PYTHON_REGION)
    write_region(full_image, vfs_image, VFS_REGION)

    output_dir.mkdir(parents=True, exist_ok=True)
    app_output = output_dir / "CardMind-cardputer-adv.bin"
    full_output = output_dir / "CardMind-cardputer-adv-full.bin"
    sums_output = output_dir / "SHA256SUMS.txt"
    shutil.copyfile(cardmind_app_path, app_output)
    full_output.write_bytes(full_image)
    sums_output.write_text(
        f"{sha256(app_output)}  {app_output.name}\n"
        f"{sha256(full_output)}  {full_output.name}\n",
        encoding="ascii",
        newline="\n",
    )
    return app_output, full_output, sums_output


def main() -> None:
    arguments = parse_arguments()
    outputs = package_images(
        arguments.cardmind_merged,
        arguments.cardmind_app,
        arguments.micropython_app,
        arguments.vfs_image,
        arguments.output_dir,
    )
    for output in outputs:
        print(output)


if __name__ == "__main__":
    main()
