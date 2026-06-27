#!/usr/bin/env python3
import argparse
import struct
import sys
from pathlib import Path

try:
    from inspect_save_trade import RESOURCE_SLOT_COUNT, SaveParser
except ImportError:
    from tools.inspect_save_trade import RESOURCE_SLOT_COUNT, SaveParser


BUILDING_OFFSETS = {
    "state": 0,
    "x": 6,
    "y": 7,
    "grid_offset": 8,
    "saved_type": 10,
    "figure_id": 34,
    "figure_id2": 36,
    "immigrant_figure_id": 38,
    "figure_id4": 40,
    "figure_spawn_delay": 42,
    "prev_part_building_id": 48,
    "next_part_building_id": 50,
    "num_workers": 56,
    "labor_category": 58,
    "flat_output_resource": 59,
    "has_road_access": 60,
    "formation_id": 72,
    "industry_progress": 74,
    "industry_stockpiling": 76,
    "industry_has_fish": 77,
    "industry_blessing_days": 78,
    "industry_orientation": 79,
    "industry_has_raw_materials": 80,
    "industry_curse_days": 81,
    "industry_age_months": 82,
    "industry_average_production_per_month": 83,
    "industry_production_current_month": 84,
    "tax_income_or_storage": 100,
    "storage_id": 109,
    "resources": 134,
    "accepted_goods": 182,
}

FIGURE_OFFSETS = {
    "type": 14,
    "resource_id": 15,
    "state": 18,
    "action_state_before_attack": 20,
    "x": 24,
    "y": 25,
    "grid_offset": 30,
    "destination_x": 32,
    "destination_y": 33,
    "destination_grid_offset": 34,
    "source_x": 36,
    "source_y": 37,
    "action_state": 44,
    "progress_on_tile": 45,
    "routing_path_id": 46,
    "routing_path_current_tile": 50,
    "routing_path_length": 54,
    "building_id": 84,
    "immigrant_building_id": 88,
    "destination_building_id": 92,
    "formation_id": 96,
    "leading_figure_id": 106,
    "loads_sold_or_carrying": 117,
    "collecting_item_id": 122,
    "target_figure_id": 130,
    "targeted_by_figure_id": 132,
    "created_sequence": 134,
    "last_destination_id": 154,
}

RAW_MATERIALS = {
    "wheat",
    "vegetables",
    "fruit",
    "meat",
    "fish",
    "clay",
    "timber",
    "olives",
    "vines",
    "iron",
    "marble",
    "gold",
    "sand",
    "stone",
}

FIGURE_SLOT_NAMES = (
    ("primary", "figure_id"),
    ("secondary", "figure_id2"),
    ("immigrant", "immigrant_figure_id"),
    ("fourth", "figure_id4"),
)


def u8(data, offset):
    return data[offset]


def i8(data, offset):
    return struct.unpack_from("<b", data, offset)[0]


def u16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def i16(data, offset):
    return struct.unpack_from("<h", data, offset)[0]


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def i32(data, offset):
    return struct.unpack_from("<i", data, offset)[0]


def resource_label(parser, slot):
    if slot is None:
        return "missing"
    return parser.resource_name(slot)


def decode_flat_resources(record):
    start = BUILDING_OFFSETS["resources"]
    resources = {}
    for slot in range(RESOURCE_SLOT_COUNT):
        value = i16(record, start + slot * 2)
        if value:
            resources[slot] = value
    return resources


def parse_building_records(parser):
    data = parser.pieces["buildings"]
    record_size = i32(data, 0)
    count = (len(data) - 4) // record_size
    details = {}
    for building_id in range(count):
        start = 4 + building_id * record_size
        record = data[start:start + record_size]
        if not record or u8(record, BUILDING_OFFSETS["state"]) == 0:
            continue
        table_info = parser.buildings["buildings"].get(building_id, {})
        details[building_id] = {
            "id": building_id,
            "state": u8(record, BUILDING_OFFSETS["state"]),
            "type": table_info.get("type", f"save_type_{u16(record, BUILDING_OFFSETS['saved_type'])}"),
            "saved_type": u16(record, BUILDING_OFFSETS["saved_type"]),
            "x": u8(record, BUILDING_OFFSETS["x"]),
            "y": u8(record, BUILDING_OFFSETS["y"]),
            "grid_offset": i16(record, BUILDING_OFFSETS["grid_offset"]),
            "figure_id": i16(record, BUILDING_OFFSETS["figure_id"]),
            "figure_id2": i16(record, BUILDING_OFFSETS["figure_id2"]),
            "immigrant_figure_id": i16(record, BUILDING_OFFSETS["immigrant_figure_id"]),
            "figure_id4": i16(record, BUILDING_OFFSETS["figure_id4"]),
            "figure_spawn_delay": u8(record, BUILDING_OFFSETS["figure_spawn_delay"]),
            "prev_part_building_id": i16(record, BUILDING_OFFSETS["prev_part_building_id"]),
            "next_part_building_id": i16(record, BUILDING_OFFSETS["next_part_building_id"]),
            "num_workers": i16(record, BUILDING_OFFSETS["num_workers"]),
            "labor_category": u8(record, BUILDING_OFFSETS["labor_category"]),
            "flat_output_resource": u8(record, BUILDING_OFFSETS["flat_output_resource"]),
            "has_road_access": u8(record, BUILDING_OFFSETS["has_road_access"]),
            "formation_id": i16(record, BUILDING_OFFSETS["formation_id"]),
            "industry_progress": i16(record, BUILDING_OFFSETS["industry_progress"]),
            "industry_stockpiling": u8(record, BUILDING_OFFSETS["industry_stockpiling"]),
            "industry_has_fish": u8(record, BUILDING_OFFSETS["industry_has_fish"]),
            "industry_blessing_days": u8(record, BUILDING_OFFSETS["industry_blessing_days"]),
            "industry_orientation": u8(record, BUILDING_OFFSETS["industry_orientation"]),
            "industry_has_raw_materials": u8(record, BUILDING_OFFSETS["industry_has_raw_materials"]),
            "industry_curse_days": u8(record, BUILDING_OFFSETS["industry_curse_days"]),
            "industry_age_months": u8(record, BUILDING_OFFSETS["industry_age_months"]),
            "industry_average_production_per_month": u8(record, BUILDING_OFFSETS["industry_average_production_per_month"]),
            "industry_production_current_month": i16(record, BUILDING_OFFSETS["industry_production_current_month"]),
            "tax_income_or_storage": i32(record, BUILDING_OFFSETS["tax_income_or_storage"]),
            "storage_id": u8(record, BUILDING_OFFSETS["storage_id"]),
            "flat_resources": decode_flat_resources(record),
        }
    return details


def figure_record_count(parser):
    data = parser.pieces["figures"]
    record_size = i32(data, 0)
    return record_size, (len(data) - 4) // record_size


def decode_figure(parser, figure_id):
    data = parser.pieces["figures"]
    record_size, count = figure_record_count(parser)
    if figure_id <= 0:
        return {"id": figure_id, "exists": False, "reason": "empty slot"}
    if figure_id >= count:
        return {"id": figure_id, "exists": False, "reason": f"outside figure table count {count}"}

    start = 4 + figure_id * record_size
    record = data[start:start + record_size]
    nonzero = any(record)
    if not nonzero:
        return {"id": figure_id, "exists": True, "nonzero": False, "alive": False, "reason": "zeroed record"}

    state = u8(record, FIGURE_OFFSETS["state"]) if len(record) > FIGURE_OFFSETS["state"] else 0
    decoded = {
        "id": figure_id,
        "exists": True,
        "nonzero": True,
        "alive": state == 1,
        "type": u8(record, FIGURE_OFFSETS["type"]),
        "state": state,
        "action_state": u8(record, FIGURE_OFFSETS["action_state"]),
        "action_state_before_attack": u8(record, FIGURE_OFFSETS["action_state_before_attack"]),
        "resource_id": u8(record, FIGURE_OFFSETS["resource_id"]),
        "resource": u8(record, FIGURE_OFFSETS["resource_id"]),
        "loads_sold_or_carrying": u8(record, FIGURE_OFFSETS["loads_sold_or_carrying"]),
        "collecting_item_id": u8(record, FIGURE_OFFSETS["collecting_item_id"]),
        "x": u8(record, FIGURE_OFFSETS["x"]),
        "y": u8(record, FIGURE_OFFSETS["y"]),
        "source_x": u8(record, FIGURE_OFFSETS["source_x"]),
        "source_y": u8(record, FIGURE_OFFSETS["source_y"]),
        "destination_x": u8(record, FIGURE_OFFSETS["destination_x"]),
        "destination_y": u8(record, FIGURE_OFFSETS["destination_y"]),
        "grid_offset": i16(record, FIGURE_OFFSETS["grid_offset"]),
        "destination_grid_offset": i16(record, FIGURE_OFFSETS["destination_grid_offset"]),
        "building_id": u32(record, FIGURE_OFFSETS["building_id"]),
        "immigrant_building_id": u32(record, FIGURE_OFFSETS["immigrant_building_id"]),
        "destination_building_id": u32(record, FIGURE_OFFSETS["destination_building_id"]),
        "routing_path_id": u32(record, FIGURE_OFFSETS["routing_path_id"]),
        "routing_path_current_tile": u32(record, FIGURE_OFFSETS["routing_path_current_tile"]),
        "routing_path_length": u32(record, FIGURE_OFFSETS["routing_path_length"]),
        "target_figure_id": u16(record, FIGURE_OFFSETS["target_figure_id"]),
        "targeted_by_figure_id": u16(record, FIGURE_OFFSETS["targeted_by_figure_id"]),
        "leading_figure_id": i16(record, FIGURE_OFFSETS["leading_figure_id"]),
        "last_destination_id": i16(record, FIGURE_OFFSETS["last_destination_id"]),
    }
    decoded["layout_warning"] = state not in (0, 1, 2) or decoded["type"] > 220
    return decoded


def decode_all_figures(parser):
    _, count = figure_record_count(parser)
    return {figure_id: decode_figure(parser, figure_id) for figure_id in range(count)}


def figure_refs_building(figure, building_id):
    return (
        figure.get("building_id") == building_id or
        figure.get("immigrant_building_id") == building_id or
        figure.get("destination_building_id") == building_id or
        figure.get("last_destination_id") == building_id
    )


def find_figure_backrefs(figures, building_id):
    refs = []
    for figure in figures.values():
        if figure.get("exists") and figure.get("nonzero") and figure_refs_building(figure, building_id):
            refs.append(figure)
    return refs


def producer_entries(parser, buildings, include_all, threshold):
    entries = []
    keyed_entries = parser.building_resources["entries"]
    for building_id, building in sorted(buildings.items()):
        resource_state = keyed_entries.get(building_id, {})
        output = resource_state.get("output")
        if output in (None, 0):
            flat_output = building.get("flat_output_resource")
            output = flat_output if flat_output else output
        if output in (None, 0):
            continue

        resources = dict(building.get("flat_resources", {}))
        resources.update(resource_state.get("resources", {}))
        stored = resources.get(output, 0)
        total_loads = sum(resources.values())
        output_name = resource_label(parser, output)
        raw_material = output_name in RAW_MATERIALS
        has_slot = any(building.get(name, 0) for _, name in FIGURE_SLOT_NAMES)
        likely = stored >= threshold or (raw_material and stored > 0 and has_slot)
        if include_all or likely:
            entries.append({
                "building": building,
                "resource_state": resource_state,
                "output": output,
                "output_name": output_name,
                "stored": stored,
                "total_loads": total_loads,
                "raw_material": raw_material,
                "likely": likely,
            })
    return entries


def format_resource_map(parser, mapping):
    if not mapping:
        return "-"
    return ", ".join(
        f"{resource_label(parser, slot)}={value}"
        for slot, value in sorted(mapping.items(), key=lambda item: resource_label(parser, item[0]))
    )


def format_building_ref(value, building_count):
    if not value:
        return "0"
    if 0 < value < building_count:
        return str(value)
    return f"invalid:{value}"


def format_figure(parser, figure, building_count):
    if not figure.get("exists"):
        return f"id={figure['id']} missing ({figure.get('reason', 'unknown')})"
    if not figure.get("nonzero", False):
        return f"id={figure['id']} zeroed_record alive=no"

    resource = resource_label(parser, figure.get("resource"))
    collect = resource_label(parser, figure.get("collecting_item_id"))
    warning = " layout_warning=yes" if figure.get("layout_warning") else ""
    return (
        f"id={figure['id']} exists=yes alive(current)={'yes' if figure.get('alive') else 'no'} "
        f"type={figure.get('type')} state={figure.get('state')} action={figure.get('action_state')} "
        f"source_building={format_building_ref(figure.get('building_id'), building_count)} "
        f"destination_building={format_building_ref(figure.get('destination_building_id'), building_count)} "
        f"last_destination={format_building_ref(figure.get('last_destination_id'), building_count)} "
        f"load={figure.get('loads_sold_or_carrying')} "
        f"resource={resource} collecting={collect} pos=({figure.get('x')},{figure.get('y')}) "
        f"dest=({figure.get('destination_x')},{figure.get('destination_y')}) "
        f"route={figure.get('routing_path_id')}/{figure.get('routing_path_current_tile')}/{figure.get('routing_path_length')}"
        f"{warning}"
    )


def print_report(parser, args):
    buildings = parse_building_records(parser)
    figures = decode_all_figures(parser)
    figure_record_size, figure_count = figure_record_count(parser)
    entries = producer_entries(parser, buildings, args.all, args.full_threshold)
    building_count = parser.buildings["count"]

    print(f"save={parser.path}")
    print(f"save_version={parser.save_version} resource_version={parser.resource_version}")
    print(
        f"building_record_size={parser.buildings['record_size']} "
        f"figure_record_size={figure_record_size} figure_records={figure_count}"
    )
    print(
        f"producer_rows={len(entries)} mode={'all_output_producers' if args.all else 'likely_stuck'} "
        f"full_threshold={args.full_threshold}"
    )
    print("figure_decode=current in-tree field offsets; layout_warning marks implausible decoded type/state fields")
    print()

    for entry in entries[:args.limit if args.limit else None]:
        building = entry["building"]
        output = entry["output"]
        resource_state = entry["resource_state"]
        resources = dict(building.get("flat_resources", {}))
        resources.update(resource_state.get("resources", {}))
        accepted = resource_state.get("accepted", {})
        flags = []
        if entry["stored"] >= args.full_threshold:
            flags.append("output_at_or_above_threshold")
        if entry["raw_material"]:
            flags.append("raw_material_output")
        if any(building.get(name, 0) for _, name in FIGURE_SLOT_NAMES):
            flags.append("has_saved_figure_slot")

        print(
            f"building={building['id']} type={building['type']} saved_type={building['saved_type']} "
            f"state={building['state']} pos=({building['x']},{building['y']}) grid={building['grid_offset']}"
        )
        print(
            f"  output={entry['output_name']} stored={entry['stored']} total_loads={entry['total_loads']} "
            f"flat_output={resource_label(parser, building.get('flat_output_resource'))} "
            f"progress={building['industry_progress']} workers={building['num_workers']} "
            f"stockpile={building['industry_stockpiling']} has_raw={building['industry_has_raw_materials']} "
            f"has_fish={building['industry_has_fish']} spawn_delay={building['figure_spawn_delay']} "
            f"road_access={building['has_road_access']} storage_id={building['storage_id']}"
        )
        print(f"  resources={format_resource_map(parser, resources)}")
        if accepted:
            print(f"  accepted={format_resource_map(parser, accepted)}")
        print(f"  flags={', '.join(flags) if flags else '-'}")

        for slot_label, field_name in FIGURE_SLOT_NAMES:
            figure_id = building.get(field_name, 0)
            if figure_id:
                figure = figures.get(figure_id, decode_figure(parser, figure_id))
                print(f"  slot {slot_label}: {format_figure(parser, figure, building_count)}")

        backrefs = [
            figure for figure in find_figure_backrefs(figures, building["id"])
            if figure["id"] not in {building.get(field_name, 0) for _, field_name in FIGURE_SLOT_NAMES}
        ]
        if backrefs:
            rendered = ", ".join(f"{figure['id']}(action={figure.get('action_state')}, alive={'yes' if figure.get('alive') else 'no'})"
                for figure in backrefs[:8])
            suffix = " ..." if len(backrefs) > 8 else ""
            print(f"  figure_backrefs_by_current_layout={rendered}{suffix}")
        print()

    if args.limit and len(entries) > args.limit:
        print(f"truncated={len(entries) - args.limit} additional rows hidden by --limit")


def main():
    ap = argparse.ArgumentParser(
        description="Inspect producer output/resource/figure state in an Augustus/Vespasian save."
    )
    ap.add_argument("save", help="Path to .sav/.svv save")
    ap.add_argument(
        "--game-root",
        default=".",
        help="Repository or installed game root containing Mods/Vespasian/Resources (default: current directory)",
    )
    ap.add_argument("--full-threshold", type=int, default=100, help="Stored output amount treated as full/stuck")
    ap.add_argument("--all", action="store_true", help="Show every building with a saved output resource")
    ap.add_argument("--limit", type=int, default=0, help="Limit printed producer rows")
    args = ap.parse_args()

    save_path = Path(args.save)
    if not save_path.exists():
        print(f"save not found: {save_path}", file=sys.stderr)
        return 2

    parser = SaveParser(save_path)
    parser.load_resources_from_xml(args.game_root)
    parser.parse()
    print_report(parser, args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
