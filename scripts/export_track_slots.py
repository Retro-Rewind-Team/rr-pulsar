#!/usr/bin/env python3
"""Export track names and slot ids from Pulsar Config*.pul files to CSV."""

from __future__ import annotations

import argparse
import csv
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


BMG_TRACKS = 0x20000
PULS_MAGIC = b"PULS"
MESG_MAGIC = b"MESGbmg1"


@dataclass(frozen=True)
class TrackRow:
    name: str
    pulsar_id: int


@dataclass(frozen=True)
class TrackEntry:
    slot_id: int
    variant_count: int


def read_u16_be(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def read_u32_be(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def parse_config_sections(data: bytes) -> tuple[int, int]:
    if data[:4] != PULS_MAGIC:
        raise ValueError("missing PULS header")

    version = read_u32_be(data, 0x4)
    if version < 0:
        raise ValueError("build-config-only files are not supported")

    cups_offset = read_u32_be(data, 0x0C)
    bmg_offset = read_u32_be(data, 0x10)
    return cups_offset, bmg_offset


def parse_track_entries(data: bytes, cups_offset: int) -> list[TrackEntry]:
    if data[cups_offset:cups_offset + 4] != b"CUPS":
        raise ValueError("missing CUPS section")

    cup_count = read_u16_be(data, cups_offset + 0x0C)
    track_count = cup_count * 4
    track_table_offset = cups_offset + 0x1C

    tracks: list[TrackEntry] = []
    for track_idx in range(track_count):
        entry_offset = track_table_offset + track_idx * 8
        slot_id = data[entry_offset]
        variant_count = read_u16_be(data, entry_offset + 2)
        tracks.append(TrackEntry(slot_id=slot_id, variant_count=variant_count))
    return tracks


def parse_bmg_blocks(data: bytes, bmg_offset: int) -> tuple[int, int, int]:
    if data[bmg_offset:bmg_offset + 8] != MESG_MAGIC:
        raise ValueError("missing MESGbmg1 section")

    block_count = read_u32_be(data, bmg_offset + 0x0C)
    offset = bmg_offset + 0x20
    inf1_offset = None
    dat1_offset = None
    mid1_offset = None

    for _ in range(block_count):
        block_magic = data[offset:offset + 4]
        block_size = read_u32_be(data, offset + 4)
        if block_magic == b"INF1":
            inf1_offset = offset
        elif block_magic == b"DAT1":
            dat1_offset = offset
        elif block_magic == b"MID1":
            mid1_offset = offset
        offset += block_size

    if inf1_offset is None or dat1_offset is None or mid1_offset is None:
        raise ValueError("BMG is missing INF1/DAT1/MID1 blocks")
    return inf1_offset, dat1_offset, mid1_offset


def decode_bmg_string(data: bytes, dat1_offset: int, string_offset: int) -> str:
    cursor = dat1_offset + 8 + string_offset
    clean: list[str] = []

    while cursor + 1 < len(data):
        codepoint = read_u16_be(data, cursor)
        if codepoint == 0:
            break

        if codepoint == 0x001A:
            if cursor + 2 >= len(data):
                break
            escape_length = data[cursor + 2]
            if escape_length == 0:
                break
            cursor += escape_length
            continue

        clean.append(chr(codepoint))
        cursor += 2
    return "".join(clean)


def parse_track_names(data: bytes, bmg_offset: int, track_count: int) -> list[str]:
    inf1_offset, dat1_offset, mid1_offset = parse_bmg_blocks(data, bmg_offset)

    msg_count = read_u16_be(data, inf1_offset + 0x08)
    entry_size = read_u16_be(data, inf1_offset + 0x0A)
    mid_count = read_u16_be(data, mid1_offset + 0x08)

    if msg_count != mid_count:
        raise ValueError("INF1 and MID1 message counts do not match")

    bmg_id_to_msg_index: dict[int, int] = {}
    for msg_index in range(mid_count):
        bmg_id = read_u32_be(data, mid1_offset + 0x10 + msg_index * 4)
        bmg_id_to_msg_index[bmg_id] = msg_index

    names: list[str] = []
    for track_idx in range(track_count):
        bmg_id = BMG_TRACKS + track_idx
        msg_index = bmg_id_to_msg_index.get(bmg_id)
        if msg_index is None:
            names.append(f"<missing bmg {bmg_id:#x}>")
            continue

        entry_offset = inf1_offset + 0x10 + msg_index * entry_size
        string_offset = read_u32_be(data, entry_offset)
        names.append(decode_bmg_string(data, dat1_offset, string_offset))
    return names


def parse_variant_name(data: bytes, bmg_offset: int, track_index: int, variant_index: int) -> str | None:
    inf1_offset, dat1_offset, mid1_offset = parse_bmg_blocks(data, bmg_offset)
    mid_count = read_u16_be(data, mid1_offset + 0x08)
    target_bmg_id = 0x420000 + (track_index << 4) + variant_index

    for msg_index in range(mid_count):
        bmg_id = read_u32_be(data, mid1_offset + 0x10 + msg_index * 4)
        if bmg_id != target_bmg_id:
            continue

        entry_size = read_u16_be(data, inf1_offset + 0x0A)
        entry_offset = inf1_offset + 0x10 + msg_index * entry_size
        string_offset = read_u32_be(data, entry_offset)
        return decode_bmg_string(data, dat1_offset, string_offset)
    return None


def build_track_name(track_name: str, variant_names: list[str]) -> str:
    all_names: list[str] = []
    for name in [track_name, *variant_names]:
        normalized_name = name.rstrip()
        if normalized_name.endswith("*"):
            normalized_name = normalized_name[:-1].rstrip()
        name = normalized_name
        if not name:
            continue
        if name in all_names:
            continue
        all_names.append(name)
    return "/".join(all_names)


def extract_rows(config_path: Path, start_index: int) -> list[TrackRow]:
    data = config_path.read_bytes()
    cups_offset, bmg_offset = parse_config_sections(data)
    tracks = parse_track_entries(data, cups_offset)
    names = parse_track_names(data, bmg_offset, len(tracks))
    rows: list[TrackRow] = []
    for local_index, track in enumerate(tracks):
        variant_names: list[str] = []
        for variant_index in range(1, track.variant_count + 1):
            variant_name = parse_variant_name(data, bmg_offset, local_index, variant_index)
            if variant_name is not None:
                variant_names.append(variant_name)

        rows.append(
            TrackRow(
                name=build_track_name(names[local_index], variant_names),
                pulsar_id=0x100 + start_index + local_index,
            )
        )
    return rows


def find_default_configs(base_dir: Path) -> list[Path]:
    return [
        path
        for path in (
            base_dir / "Binaries" / "ConfigRT.pul",
            base_dir / "Binaries" / "ConfigCT.pul",
            base_dir / "Binaries" / "ConfigBT.pul",
        )
        if path.is_file()
    ]


def write_csv(rows: list[TrackRow], output_path: Path, hex_ids: bool) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="", encoding="utf-8") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(("track_name", "pulsar_id"))
        for row in rows:
            id_value = f"0x{row.pulsar_id:03X}" if hex_ids else str(row.pulsar_id)
            writer.writerow((row.name, id_value))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Export track names and slot ids from Retro Rewind Config*.pul files."
    )
    parser.add_argument(
        "paths",
        nargs="*",
        help="Config*.pul files or a directory containing ConfigRT.pul, ConfigCT.pul, and ConfigBT.pul.",
    )
    parser.add_argument(
        "-o",
        "--output",
        default="track_slots.csv",
        help="Output CSV path. Default: track_slots.csv",
    )
    parser.add_argument(
        "--decimal-slots",
        action="store_true",
        help="Write Pulsar ids as decimal instead of hexadecimal.",
    )
    return parser.parse_args()


def resolve_inputs(raw_paths: list[str]) -> list[Path]:
    if not raw_paths:
        return find_default_configs(Path("RetroRewind6"))

    resolved: list[Path] = []
    for raw_path in raw_paths:
        path = Path(raw_path)
        if path.is_dir():
            resolved.extend(find_default_configs(path))
        elif path.is_file():
            resolved.append(path)
        else:
            raise FileNotFoundError(raw_path)

    unique_paths: list[Path] = []
    seen: set[Path] = set()
    for path in resolved:
        normalized = path.resolve()
        if normalized not in seen:
            seen.add(normalized)
            unique_paths.append(path)
    return unique_paths


def main() -> int:
    args = parse_args()

    try:
        input_paths = resolve_inputs(args.paths)
        if not input_paths:
            raise FileNotFoundError("no Config*.pul files found")
    except FileNotFoundError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    rows: list[TrackRow] = []
    start_index = 0
    for input_path in input_paths:
        config_rows = extract_rows(input_path, start_index)
        rows.extend(config_rows)
        start_index += len(config_rows)

    output_path = Path(args.output)
    write_csv(rows, output_path, hex_ids=not args.decimal_slots)
    print(f"wrote {len(rows)} rows to {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
