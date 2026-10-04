#!/usr/bin/env python3
"""Extract and categorize Bad Toys 3D original assets.

The original game stores most runtime assets in data.pck as a DatPack archive.
This script extracts each entry to a stable category folder and writes a JSON
manifest with original names, offsets, sizes, and output paths.
"""

from __future__ import annotations

import argparse
import json
import shutil
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Entry:
    name: str
    offset: int
    length: int


ENEMY_PREFIXES = {
    "BLB": "blobs",
    "ELK": "electric",
    "KOU": "balls",
    "PNS": "dogs",
    "PRK": "parkers",
    "PSK": "projectiles",
    "RBT": "robots",
    "RCK": "rockets",
    "ROB": "robo_parts",
    "VOS": "wasps",
}


def chunk(tag: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + tag
        + payload
        + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
    )


def write_png(path: Path, width: int, height: int, pixels: list[tuple[int, int, int, int]]) -> None:
    if width <= 0 or height <= 0 or len(pixels) != width * height:
        raise ValueError(f"Invalid PNG dimensions for {path}")

    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)
        for r, g, b, a in pixels[y * width : (y + 1) * width]:
            scanlines.extend((r, g, b, a))

    payload = b"".join(
        [
            b"\x89PNG\r\n\x1a\n",
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)),
            chunk(b"IDAT", zlib.compress(bytes(scanlines), level=9)),
            chunk(b"IEND", b""),
        ]
    )
    path.write_bytes(payload)


def palette_from_payload(payload: bytes) -> list[tuple[int, int, int, int]]:
    colors: list[tuple[int, int, int, int]] = []
    for index in range(256):
        base = index * 3
        if base + 2 < len(payload):
            colors.append((payload[base], payload[base + 1], payload[base + 2], 255))
        else:
            colors.append((0, 0, 0, 255))
    return colors


def transparent_blue(color: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    r, g, b, _ = color
    if (r == 0 and g == 0 and b == 100) or (b > 40 and b >= r * 1.8 and b >= g * 1.8):
        return (r, g, b, 0)
    return color


def set_pixel(
    pixels: list[tuple[int, int, int, int]],
    width: int,
    height: int,
    top_down: bool,
    x: int,
    y: int,
    color: tuple[int, int, int, int],
) -> None:
    if 0 <= x < width and 0 <= y < height:
        dst_y = y if top_down else height - 1 - y
        pixels[dst_y * width + x] = color


def decode_bmp(payload: bytes) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    if len(payload) < 54 or payload[:2] != b"BM":
        raise ValueError("Not a BMP payload")

    pixel_offset = struct.unpack_from("<I", payload, 10)[0]
    dib_size = struct.unpack_from("<I", payload, 14)[0]
    width = struct.unpack_from("<i", payload, 18)[0]
    height_value = struct.unpack_from("<i", payload, 22)[0]
    bits_per_pixel = struct.unpack_from("<H", payload, 28)[0]
    compression = struct.unpack_from("<I", payload, 30)[0]
    if dib_size < 40 or width <= 0 or height_value == 0:
        raise ValueError("Unsupported BMP dimensions")
    if not ((bits_per_pixel == 4 and compression in {0, 2}) or (bits_per_pixel == 8 and compression in {0, 1})):
        raise ValueError("Unsupported BMP encoding")

    top_down = height_value < 0
    height = -height_value if top_down else height_value
    palette_entries = 1 << bits_per_pixel
    palette_offset = 14 + dib_size
    palette_size = palette_entries * 4
    row_bytes = ((width * bits_per_pixel + 31) // 32) * 4
    if palette_offset + palette_size > len(payload):
        raise ValueError("BMP palette outside payload")
    if compression == 0 and pixel_offset + row_bytes * height > len(payload):
        raise ValueError("BMP pixels outside payload")

    palette: list[tuple[int, int, int, int]] = []
    for index in range(palette_entries):
        base = palette_offset + index * 4
        palette.append(transparent_blue((payload[base + 2], payload[base + 1], payload[base], 255)))

    pixels = [(0, 0, 0, 0)] * (width * height)
    if compression == 0:
        for row in range(height):
            src_row = row if top_down else height - 1 - row
            row_start = pixel_offset + src_row * row_bytes
            for x in range(width):
                if bits_per_pixel == 8:
                    index = payload[row_start + x]
                else:
                    packed = payload[row_start + x // 2]
                    index = packed >> 4 if x % 2 == 0 else packed & 0x0F
                pixels[row * width + x] = palette[index]
        return width, height, pixels

    x = 0
    y = 0

    def put_next_pixel(color: tuple[int, int, int, int]) -> None:
        nonlocal x, y
        set_pixel(pixels, width, height, top_down, x, y, color)
        x += 1
        if x >= width:
            x = 0
            y += 1

    byte_index = pixel_offset
    while byte_index < len(payload) and y < height:
        count = payload[byte_index]
        byte_index += 1
        if byte_index >= len(payload):
            break
        if count > 0:
            value = payload[byte_index]
            byte_index += 1
            for i in range(count):
                if bits_per_pixel == 8:
                    index = value
                else:
                    index = (value >> 4) & 0x0F if i % 2 == 0 else value & 0x0F
                put_next_pixel(palette[index])
                if y >= height:
                    break
            continue

        command = payload[byte_index]
        byte_index += 1
        if command == 0:
            x = 0
            y += 1
        elif command == 1:
            break
        elif command == 2:
            if byte_index + 1 >= len(payload):
                break
            x += payload[byte_index]
            y += payload[byte_index + 1]
            byte_index += 2
        elif bits_per_pixel == 8:
            literal_count = command
            for _ in range(literal_count):
                if byte_index >= len(payload):
                    break
                index = payload[byte_index]
                byte_index += 1
                put_next_pixel(palette[index])
            if literal_count & 1:
                byte_index += 1
        else:
            literal_pixels = command
            literal_bytes = (literal_pixels + 1) // 2
            for i in range(literal_pixels):
                if byte_index + i // 2 >= len(payload):
                    break
                packed = payload[byte_index + i // 2]
                index = (packed >> 4) & 0x0F if i % 2 == 0 else packed & 0x0F
                put_next_pixel(palette[index])
            byte_index += literal_bytes + (literal_bytes & 1)

    return width, height, pixels


def rotate_ccw(
    width: int, height: int, pixels: list[tuple[int, int, int, int]]
) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    rotated = [(0, 0, 0, 0)] * (width * height)
    for y in range(height):
        for x in range(width):
            dst_x = y
            dst_y = width - 1 - x
            rotated[dst_y * height + dst_x] = pixels[y * width + x]
    return height, width, rotated


def decode_stn(payload: bytes, palette: list[tuple[int, int, int, int]]) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    if len(payload) != 64 * 64:
        raise ValueError("STN texture is not 64x64")
    pixels = [palette[index] for index in payload]
    return rotate_ccw(64, 64, pixels)


def decode_patch(payload: bytes, palette: list[tuple[int, int, int, int]]) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    if len(payload) < 0x100:
        raise ValueError("Patch payload is too small")
    pixels = [(0, 0, 0, 0)] * (64 * 64)
    for column in range(64):
        offset = payload[column * 2] | (payload[column * 2 + 1] << 8)
        start_y = payload[0x80 + column * 2]
        run_length = payload[0x80 + column * 2 + 1]
        for y in range(run_length):
            target_y = start_y + y
            if 0 <= target_y < 64 and offset + y < len(payload):
                palette_index = payload[offset + y]
                if palette_index != 255:
                    pixels[target_y * 64 + column] = palette[palette_index]
    return 64, 64, pixels


def read_datpack(path: Path) -> tuple[list[Entry], bytes]:
    data = path.read_bytes()
    if len(data) < 16:
        raise ValueError(f"{path} is too small to be a DatPack archive")

    signature_length = data[0]
    signature = data[1 : 1 + signature_length].decode("ascii", errors="replace")
    if signature != "DatPack":
        raise ValueError(f"{path} has signature {signature!r}, expected 'DatPack'")

    directory_offset, entry_count = struct.unpack_from("<II", data, 8)
    entries: list[Entry] = []
    for index in range(entry_count):
        entry_offset = directory_offset + index * 16
        if entry_offset + 16 > len(data):
            raise ValueError(f"DatPack directory entry {index} is outside the file")
        raw_name, asset_offset, asset_length = struct.unpack_from("<8sII", data, entry_offset)
        name = raw_name.decode("latin1").rstrip(" ")
        if asset_offset + asset_length > len(data):
            raise ValueError(f"DatPack entry {name} points outside the file")
        entries.append(Entry(name=name, offset=asset_offset, length=asset_length))
    return entries, data


def category_for(name: str, payload: bytes) -> tuple[str, str]:
    prefix = name.split("_", 1)[0]

    if name == "BT_PAL":
        return "palette", ".pal"
    if name.startswith("SND_") or payload.startswith(b"RIFF"):
        return "audio/sounds", ".wav"
    if name.startswith("MAP_"):
        return "levels", ".map242"
    if name.startswith("STN_"):
        return "textures/walls", ".stn"
    if name.startswith("VEC_"):
        return "sprites/patches", ".vec"
    if prefix in ENEMY_PREFIXES:
        return f"sprites/enemies/{ENEMY_PREFIXES[prefix]}", ".sprite"
    if name.startswith("BM_") or name.startswith("HS_") or name.startswith("KLV_"):
        return "ui/bitmaps", ".bmp" if payload.startswith(b"BM") else ".bin"
    if name.startswith("M_"):
        return "ui/map-icons", ".bin"
    if name.startswith("DEM_"):
        return "demos", ".demo"
    if name in {"README", "REGFORM", "NEW"}:
        return "docs/packed", ".bin"
    return "misc", ".bin"


def decode_image(
    name: str,
    payload: bytes,
    palette: list[tuple[int, int, int, int]],
) -> tuple[int, int, list[tuple[int, int, int, int]]] | None:
    prefix = name.split("_", 1)[0]
    if payload.startswith(b"BM"):
        return decode_bmp(payload)
    if name.startswith("STN_"):
        return decode_stn(payload, palette)
    if name.startswith("VEC_") or prefix in ENEMY_PREFIXES:
        return decode_patch(payload, palette)
    return None


def write_entry(
    out_root: Path,
    entry: Entry,
    payload: bytes,
    palette: list[tuple[int, int, int, int]],
    render_png: bool,
) -> dict[str, object]:
    category, extension = category_for(entry.name, payload)
    output_path = out_root / category / f"{entry.name}{extension}"
    rendered = False
    if render_png:
        image = decode_image(entry.name, payload, palette)
        if image is not None:
            width, height, pixels = image
            output_path = out_root / category / f"{entry.name}.png"
            rendered = True
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if rendered:
        write_png(output_path, width, height, pixels)
    else:
        output_path.write_bytes(payload)
    return {
        "name": entry.name,
        "category": category,
        "offset": entry.offset,
        "length": entry.length,
        "path": output_path.relative_to(out_root).as_posix(),
        "rendered_png": rendered,
    }


def copy_loose_original_assets(source_dir: Path, out_root: Path) -> list[dict[str, object]]:
    copied: list[dict[str, object]] = []
    loose_assets = {
        "m1.dat": ("audio/music", ".mid"),
        "readme.txt": ("docs/loose", ".txt"),
        "regform.txt": ("docs/loose", ".txt"),
    }

    for filename, (category, extension) in loose_assets.items():
        source_path = source_dir / filename
        if not source_path.exists():
            continue
        output_path = out_root / category / f"{source_path.stem}{extension}"
        output_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source_path, output_path)
        copied.append(
            {
                "name": filename,
                "category": category,
                "offset": None,
                "length": source_path.stat().st_size,
                "path": output_path.relative_to(out_root).as_posix(),
            }
        )
    return copied


def extract(source_dir: Path, out_root: Path, clean: bool, render_png: bool) -> dict[str, object]:
    pack_path = source_dir / "data.pck"
    if not pack_path.exists():
        raise FileNotFoundError(f"Missing {pack_path}")

    if clean and out_root.exists():
        shutil.rmtree(out_root)
    out_root.mkdir(parents=True, exist_ok=True)

    entries, archive = read_datpack(pack_path)
    palette_payload = b""
    for entry in entries:
        if entry.name == "BT_PAL":
            palette_payload = archive[entry.offset : entry.offset + entry.length]
            break
    palette = palette_from_payload(palette_payload)

    manifest_entries = []
    for entry in entries:
        payload = archive[entry.offset : entry.offset + entry.length]
        manifest_entries.append(write_entry(out_root, entry, payload, palette, render_png))

    manifest_entries.extend(copy_loose_original_assets(source_dir, out_root))

    manifest = {
        "source": str(source_dir),
        "pack": str(pack_path),
        "entry_count": len(entries),
        "extracted_count": len(manifest_entries),
        "rendered_png_count": sum(1 for entry in manifest_entries if entry.get("rendered_png")),
        "entries": manifest_entries,
    }
    (out_root / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def default_repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def parse_args() -> argparse.Namespace:
    repo_root = default_repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source",
        type=Path,
        default=repo_root / "original",
        help="Folder containing original/data.pck and optional loose files",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=repo_root / "extracted-assets",
        help="Destination folder for categorized assets",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Remove the destination folder before extracting",
    )
    parser.add_argument(
        "--raw",
        action="store_true",
        help="Keep image assets in original raw formats instead of rendering PNG files",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest = extract(args.source, args.out, args.clean, not args.raw)
    print(f"Extracted {manifest['extracted_count']} assets to {args.out}")
    print(f"Rendered {manifest['rendered_png_count']} image assets as PNG")
    print(f"Wrote {args.out / 'manifest.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
