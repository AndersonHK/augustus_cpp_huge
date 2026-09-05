#!/usr/bin/env python3
import argparse
import shutil
import struct
import sys
from datetime import datetime
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.inspect_save_trade import SaveParser, UNCOMPRESSED

HIPPO_OFFSETS = {
    0: ((5, 0), (10, 0)),
    1: ((0, -5), (0, -10)),
    2: ((-5, 0), (-10, 0)),
    3: ((0, 5), (0, 10)),
}


def parse_save(path):
    parser = SaveParser(path)
    parser.load_resources_from_xml(str(REPO_ROOT))
    parser.parse()
    return parser


def patch_requests(parser):
    raw = bytearray(parser.pieces["requests"])
    fish = next((save_id for save_id, text in parser.resource_table.items() if text == "fish"), None)
    meat = next((save_id for save_id, text in parser.resource_table.items() if text == "meat"), None)
    if fish is None or meat is None or len(raw) < 16:
        return raw, 0

    total_size, _, count, record_size = struct.unpack_from("<IIII", raw, 0)
    if total_size != len(raw) or record_size < 4 or 16 + count * record_size > len(raw):
        raise RuntimeError("requests chunk has an unexpected dynamic-array layout")

    changed = 0
    for index in range(count):
        resource_offset = 16 + index * record_size + 2
        if struct.unpack_from("<h", raw, resource_offset)[0] == fish:
            struct.pack_into("<h", raw, resource_offset, meat)
            changed += 1
    return raw, changed


def patch_hippodromes(parser):
    raw = bytearray(parser.pieces["buildings"])
    if len(raw) < 4:
        return raw, 0

    record_size = struct.unpack_from("<i", raw, 0)[0]
    if record_size <= 16 or (len(raw) - 4) % record_size:
        raise RuntimeError("buildings chunk has an unexpected flat-record layout")

    count = (len(raw) - 4) // record_size
    records = []
    by_kind_and_xy = {}
    for building_id in range(count):
        offset = 4 + building_id * record_size
        if raw[offset] == 0:
            continue
        saved_type = struct.unpack_from("<H", raw, offset + 10)[0]
        kind = parser.building_table.get(saved_type)
        if kind not in {"hippodrome", "hippodrome_middle", "hippodrome_end"}:
            continue
        record = {
            "id": building_id,
            "offset": offset,
            "kind": kind,
            "x": raw[offset + 6],
            "y": raw[offset + 7],
        }
        records.append(record)
        by_kind_and_xy.setdefault((kind, record["x"], record["y"]), []).append(record)

    changed = 0
    for main in [record for record in records if record["kind"] == "hippodrome"]:
        best_rotation = None
        best_parts = []
        for rotation, (middle_delta, end_delta) in HIPPO_OFFSETS.items():
            middle = by_kind_and_xy.get(
                ("hippodrome_middle", main["x"] + middle_delta[0], main["y"] + middle_delta[1]), [])
            end = by_kind_and_xy.get(
                ("hippodrome_end", main["x"] + end_delta[0], main["y"] + end_delta[1]), [])
            parts = middle[:1] + end[:1]
            if len(parts) > len(best_parts):
                best_rotation = rotation
                best_parts = parts
        if best_rotation is None or not best_parts:
            continue
        for record in [main] + best_parts:
            orientation_offset = record["offset"] + 12
            if struct.unpack_from("<h", raw, orientation_offset)[0] != best_rotation:
                struct.pack_into("<h", raw, orientation_offset, best_rotation)
                changed += 1
    return raw, changed


def dynamic_uncompressed_chunk(payload):
    return struct.pack("<i", len(payload)) + payload


def dynamic_compressed_piece_as_uncompressed(payload):
    return struct.pack("<iI", len(payload), UNCOMPRESSED) + payload


def apply_replacements(data, replacements):
    for start, end, chunk in sorted(replacements, reverse=True):
        data = data[:start] + chunk + data[end:]
    return data


def patch_save(path):
    parser = parse_save(path)
    requests, request_changes = patch_requests(parser)
    buildings, hippodrome_changes = patch_hippodromes(parser)

    replacements = []
    if requests != parser.pieces["requests"]:
        start, end = parser.piece_offsets["requests"]
        replacements.append((start, end, dynamic_uncompressed_chunk(requests)))
    if buildings != parser.pieces["buildings"]:
        start, end = parser.piece_offsets["buildings"]
        replacements.append((start, end, dynamic_compressed_piece_as_uncompressed(buildings)))

    original = path.read_bytes()
    patched = apply_replacements(bytearray(original), replacements)
    if patched != original:
        backup = path.with_name(f"{path.name}.{datetime.now():%Y%m%d-%H%M%S}.bak")
        shutil.copy2(path, backup)
        path.write_bytes(patched)
    else:
        backup = None

    return {
        "backup": backup,
        "fish_requests_changed": request_changes,
        "hippodrome_orientations_changed": hippodrome_changes,
        "written": patched != original,
    }


def main():
    ap = argparse.ArgumentParser(description="Fix Procurator Attempt 1 7 fish requests and hippodrome orientation.")
    ap.add_argument("save", type=Path, help="Path to the save file to repair.")
    args = ap.parse_args()

    path = args.save
    if not path.is_file():
        raise SystemExit(f"Save not found: {path}")

    result = patch_save(path)
    print(f"save={path}")
    print(f"written={result['written']}")
    print(f"backup={result['backup'] or '-'}")
    print(f"fish_requests_changed={result['fish_requests_changed']}")
    print(f"hippodrome_orientations_changed={result['hippodrome_orientations_changed']}")


if __name__ == "__main__":
    main()
