#!/usr/bin/env python3
import argparse
import re
import struct
import zlib
from pathlib import Path
from xml.etree import ElementTree

GRID_SIZE = 162
GRID_U8 = GRID_SIZE * GRID_SIZE
GRID_U16 = GRID_U8 * 2
GRID_U32 = GRID_U8 * 4
RESOURCE_SLOT_COUNT = 24
UNCOMPRESSED = 0x80000000

SAVE_GAME_LAST_NO_RESOURCE_TYPE_TABLE = 0xB8
SAVE_GAME_LAST_NO_BUILDING_TYPE_TABLE = 0xB4
SAVE_GAME_LAST_NO_WATER_ACCESS_TYPE_TABLE = 0xB7
SAVE_GAME_LAST_NO_GOD_TYPE_TABLE = 0xB9
SAVE_GAME_LAST_NO_KEYED_RESOURCE_STATE = 0xB8
SAVE_GAME_LAST_NO_ROAD_SERVICE_HISTORY = 0xAF
SAVE_GAME_LAST_NO_LOCAL_WORKFORCE = 0xB1
SAVE_GAME_LAST_NO_FORMULAS_AND_MODEL_DATA = 0xA9
SAVE_GAME_LAST_NO_MARKET_ROAD_SERVICE_HISTORY = 0xB5
SAVE_GAME_LAST_NO_MOD_METADATA = 0xAE
SAVE_GAME_LAST_NO_CUSTOM_CAMPAIGNS = 0x9D
SAVE_GAME_LAST_GLOBAL_BUILDING_INFO = 0x91
SAVE_GAME_LAST_U16_GRIDS = 0xA8
SAVE_GAME_LAST_STATIC_SCENARIO_ORIGINAL_DATA = 0x9E
SAVE_GAME_LAST_NO_CUSTOM_EMPIRE_MAP_IMAGE = 0x9C
SAVE_GAME_LAST_LIMITED_ROUTE_COST = 0xAC
SAVE_GAME_LAST_NO_EXTENDED_REQUESTS = 0x94
SAVE_GAME_LAST_NO_EVENTS = 0x95
SAVE_GAME_LAST_NO_CUSTOM_MESSAGES = 0x96
SAVE_GAME_LAST_NO_DELIVERIES_VERSION = 0x77
SAVE_GAME_LAST_STATIC_VERSION = 0x78
SAVE_GAME_LAST_STATIC_BUILDING_COUNT_VERSION = 0x80
SAVE_GAME_LAST_BARRACKS_TOWER_SENTRY_REQUEST = 0x8A
SAVE_GAME_LAST_STATIC_SCENARIO_OBJECTS = 0x93
SAVE_GAME_LAST_UNKNOWN_UNUSED_CITY_DATA = 0x8F
SAVE_GAME_LAST_STORED_IMAGE_IDS = 0x83
SAVE_GAME_LAST_ORIGINAL_TERRAIN_DATA_SIZE_VERSION = 0x86
SAVE_GAME_LAST_SMALLER_IMAGE_ID_VERSION = 0x76

LEGACY_MONUMENT_RESOURCES = [
    "none", "wheat", "vegetables", "fruit", "meat", "fish",
    "clay", "timber", "olives", "vines", "iron", "marble", "gold", "sand", "stone",
    "pottery", "furniture", "oil", "wine", "weapons", "concrete", "bricks",
    "denarii", "troops",
]


class Reader:
    def __init__(self, data):
        self.data = data
        self.i = 0

    def u8(self):
        value = self.data[self.i]
        self.i += 1
        return value

    def i8(self):
        value = struct.unpack_from("<b", self.data, self.i)[0]
        self.i += 1
        return value

    def u16(self):
        value = struct.unpack_from("<H", self.data, self.i)[0]
        self.i += 2
        return value

    def i16(self):
        value = struct.unpack_from("<h", self.data, self.i)[0]
        self.i += 2
        return value

    def u32(self):
        value = struct.unpack_from("<I", self.data, self.i)[0]
        self.i += 4
        return value

    def i32(self):
        value = struct.unpack_from("<i", self.data, self.i)[0]
        self.i += 4
        return value

    def bytes(self, size):
        out = self.data[self.i:self.i + size]
        self.i += size
        return out

    def skip(self, size):
        self.i += size


class SaveParser:
    def __init__(self, path):
        self.path = Path(path)
        self.reader = Reader(self.path.read_bytes())
        self.pieces = {}
        self.piece_offsets = {}
        self.resource_table = {i: name for i, name in enumerate(LEGACY_MONUMENT_RESOURCES[:22])}
        self.resource_text_to_slot = {}
        self.resource_slot_to_text = {}
        self.resource_special = set()
        self.resource_total_mapped = 22
        self.building_table = {}

    def load_resources_from_xml(self, root):
        for file in sorted((Path(root) / "Mods" / "Vespasian" / "Resources").glob("*.xml")):
            try:
                resource = ElementTree.parse(file).getroot()
            except ElementTree.ParseError:
                continue
            text_id = resource.attrib.get("id")
            slot = int(resource.attrib.get("slot", "-1"))
            if not text_id or slot < 0:
                continue
            self.resource_text_to_slot[text_id] = slot
            self.resource_slot_to_text[slot] = text_id
            model = resource.find("model")
            if model is not None and "special" in model.attrib.get("flags", ""):
                self.resource_special.add(slot)

    def save_id_to_slot(self, save_id):
        text_id = self.resource_table.get(save_id)
        if text_id is None:
            return None
        return self.resource_text_to_slot.get(text_id)

    def resource_name(self, slot):
        if slot is None:
            return "missing"
        return self.resource_slot_to_text.get(slot, f"slot_{slot}")

    def read_piece(self, name, size, compressed):
        start = self.reader.i
        dynamic = size is None
        if dynamic:
            size = self.reader.i32()
            if size == 0:
                self.pieces[name] = b""
                self.piece_offsets[name] = (start, self.reader.i)
                return b""
        if compressed:
            input_size = self.reader.u32()
            if input_size == UNCOMPRESSED:
                data = self.reader.bytes(size)
            else:
                compressed_data = self.reader.bytes(input_size)
                data = zlib.decompress(compressed_data)
                if len(data) != size:
                    raise ValueError(f"{name}: expected {size} bytes, decompressed {len(data)}")
        else:
            data = self.reader.bytes(size)
        self.pieces[name] = data
        self.piece_offsets[name] = (start, self.reader.i)
        return data

    def parse_resource_table(self, data):
        r = Reader(data)
        total_size = r.u32()
        version = r.u32()
        count = r.u32()
        table = {0: "none"}
        for _ in range(count):
            save_id = r.u16()
            text_len = r.u16()
            text_id = r.bytes(text_len).decode("utf-8", errors="replace")
            table[save_id] = text_id
        self.resource_table = table
        max_save_id = 0
        for save_id, text_id in table.items():
            slot = self.resource_text_to_slot.get(text_id)
            if slot == 0 or slot not in self.resource_special:
                max_save_id = max(max_save_id, save_id)
        self.resource_total_mapped = max_save_id + 1
        return {"total_size": total_size, "version": version, "count": count, "table": table}

    def parse_building_type_table(self, data):
        r = Reader(data)
        r.u32()
        version = r.u32()
        count = r.u32()
        table = {}
        for _ in range(count):
            save_id = r.u16()
            text_len = r.u16()
            text_id = r.bytes(text_len).decode("utf-8", errors="replace")
            table[save_id] = text_id
        self.building_table = table
        return {"version": version, "count": count, "table": table}

    def parse_empire_cities(self, data):
        r = Reader(data)
        record_size = r.i32()
        resources_to_load = (record_size - 20) // 2
        cities = []
        for city_id in range((len(data) - 4) // record_size):
            start = r.i
            city = {
                "id": city_id,
                "in_use": r.u8(),
                "type": r.u8(),
                "name_id": r.u8(),
                "route_id": r.u8(),
                "is_open": r.u8(),
                "buys": {},
                "sells": {},
            }
            for save_id in range(resources_to_load):
                value = r.u8()
                slot = self.save_id_to_slot(save_id)
                if value and slot is not None:
                    city["buys"][slot] = value
            for save_id in range(resources_to_load):
                value = r.u8()
                slot = self.save_id_to_slot(save_id)
                if value and slot is not None:
                    city["sells"][slot] = value
            city["cost_to_open"] = r.u32()
            city["trader_entry_delay"] = r.i16()
            city["empire_object_id"] = r.i16()
            city["is_sea_trade"] = r.u8()
            city["trader_figure_ids"] = [r.i16() for _ in range(3)]
            r.i = start + record_size
            if city["in_use"]:
                cities.append(city)
        return {"record_size": record_size, "resources_to_load": resources_to_load, "cities": cities}

    def parse_trade_routes(self, data):
        r = Reader(data)
        count = r.i32()
        resource_count = 0
        if count > 0 and len(data) >= 4:
            divisor = count * 2 * 2 * 4
            resource_count = (len(data) - 4) // divisor if divisor else 0
        self.trade_route_resource_count = resource_count
        routes = []
        for route_id in range(count):
            route = {"id": route_id, "sells": {}, "buys": {}}
            for buying in (0, 1):
                bucket = "buys" if buying else "sells"
                for save_id in range(resource_count):
                    limit = r.i32()
                    traded = r.i32()
                    slot = self.save_id_to_slot(save_id)
                    if slot is not None and (limit or traded):
                        route[bucket][slot] = {"limit": limit, "traded": traded}
            routes.append(route)
        return routes

    def parse_building_resource_state(self, data):
        r = Reader(data)
        r.u32()
        version = r.u32()
        count = r.u32()
        entries = {}
        for _ in range(count):
            building_id = r.u32()
            entry = {
                "output": self.save_id_to_slot(r.u16()),
                "warehouse_resource": self.save_id_to_slot(r.u16()),
                "fetch_inventory": self.save_id_to_slot(r.u16()),
                "depot_order": self.save_id_to_slot(r.u16()),
                "resources": {},
                "accepted": {},
            }
            resource_count = r.u32()
            for _ in range(resource_count):
                slot = self.save_id_to_slot(r.u16())
                value = r.i16()
                if slot is not None and value:
                    entry["resources"][slot] = value
            accepted_count = r.u32()
            for _ in range(accepted_count):
                slot = self.save_id_to_slot(r.u16())
                value = r.u8()
                if slot is not None and value:
                    entry["accepted"][slot] = value
            entries[building_id] = entry
        return {"version": version, "count": count, "entries": entries}

    def parse_buildings(self, data):
        r = Reader(data)
        record_size = r.i32()
        count = (len(data) - 4) // record_size
        buildings = {}
        for building_id in range(count):
            start = r.i
            state = r.u8()
            r.skip(7)
            r.i16()
            saved_type = r.u16()
            r.i = start + record_size
            if state:
                buildings[building_id] = {
                    "id": building_id,
                    "state": state,
                    "saved_type": saved_type,
                    "type": self.building_table.get(saved_type, f"save_type_{saved_type}"),
                }
        return {"record_size": record_size, "count": count, "buildings": buildings}

    def parse_storage_state(self, data):
        r = Reader(data)
        record_size = r.i32()
        num_resources = (record_size - 10) // 2
        storages = []
        for storage_id in range((len(data) - 4) // record_size):
            storage = {
                "id": storage_id,
                "permissions": r.i32(),
                "building_id": r.i32(),
                "in_use": r.u8(),
                "empty_all": r.u8(),
                "resources": {},
            }
            for save_id in range(num_resources):
                state = r.u8()
                quantity = r.u8()
                slot = self.save_id_to_slot(save_id)
                if slot is not None and (state or quantity):
                    storage["resources"][slot] = {"state": state, "quantity": quantity}
            if storage["in_use"]:
                storages.append(storage)
        return {"record_size": record_size, "num_resources": num_resources, "storages": storages}

    def parse_demand_changes(self, data):
        if not data:
            return {"count": 0, "element_size": 0, "changes": []}
        r = Reader(data)
        total_size = r.u32()
        r.u32()
        count = r.u32()
        element_size = r.u32()
        changes = []
        for change_id in range(count):
            start = r.i
            change = {
                "id": change_id,
                "year": r.i16(),
                "month": r.u8(),
                "resource": r.u8(),
                "route_id": r.u8(),
                "amount": r.i32(),
                "buys": r.u8(),
            }
            r.i = start + element_size
            if change["year"]:
                changes.append(change)
        return {
            "total_size": total_size,
            "count": count,
            "element_size": element_size,
            "changes": changes,
        }

    def parse(self):
        save_version = struct.unpack_from("<i", self.reader.data, 4)[0]
        resource_version = struct.unpack_from("<i", self.reader.data, 8)[0]
        self.save_version = save_version
        self.resource_version = resource_version
        current_resource_total = 22

        pieces = [
            ("scenario_campaign_mission", 4, False),
            ("file_version", 4, False),
            ("resource_version", 4, False),
            ("resource_type_table", None, False),
            ("scenario_version", 4, False),
            ("edge_grid", GRID_U8, True),
            ("building_grid", GRID_U32, True),
            ("terrain_grid", GRID_U32, True),
            ("aqueduct_grid", GRID_U8, True),
            ("figure_grid", GRID_U16, True),
            ("bitfields_grid", GRID_U16, True),
            ("sprite_grid", GRID_U8, True),
            ("random_grid", GRID_U8, False),
            ("desirability_grid", GRID_U8, True),
            ("elevation_grid", GRID_U8, True),
            ("building_damage_grid", GRID_U8, True),
            ("aqueduct_backup_grid", GRID_U8, True),
            ("sprite_backup_grid", GRID_U8, True),
            ("figures", None, True),
            ("route_figures", None, True),
            ("route_paths", None, True),
            ("formations", None, True),
            ("formation_totals", 12, False),
            ("city_data", None, True),
            ("player_name", 64, False),
            ("building_type_table", None, False),
            ("water_access_type_table", None, False),
            ("god_type_table", None, False),
            ("buildings", None, True),
            ("building_resource_state", None, True),
            ("city_view_orientation", 4, False),
            ("game_time", 20, False),
            ("building_extra_highest_id_ever", 8, False),
            ("random_iv", 8, False),
            ("city_view_camera", 8, False),
            ("city_graph_order", 4, False),
            ("emperor_change_time", 8, False),
            ("empire", 12, False),
            ("empire_map", None, False),
            ("empire_cities", None, True),
            ("trade_prices", 8 * current_resource_total, False),
            ("figure_names", 84, False),
            ("culture_coverage", 60, False),
        ]

        for name, size, compressed in pieces:
            data = self.read_piece(name, size, compressed)
            if name == "resource_type_table":
                self.resource_table_info = self.parse_resource_table(data)
            elif name == "building_type_table":
                self.building_type_table_info = self.parse_building_type_table(data)

        scenario_size = struct.unpack_from("<i", self.reader.data, self.reader.i)[0]
        self.read_piece("scenario", scenario_size, False)

        after_scenario = [
            ("requests", None, False),
            ("invasions", None, True),
            ("demand_changes", None, True),
            ("price_changes", None, True),
            ("allowed_buildings", None, True),
            ("custom_variables", None, True),
            ("scenario_events", None, False),
            ("scenario_formulas", None, False),
            ("scenario_conditions", None, False),
            ("scenario_actions", None, False),
            ("custom_messages", None, False),
            ("custom_media", None, False),
            ("message_media_text_blob", None, False),
            ("message_media_metadata", None, False),
            ("building_model_data", None, False),
            ("max_game_year", 4, False),
            ("earthquake", 60, False),
            ("emperor_change_state", 4, False),
            ("messages", 16000, True),
            ("message_extra", 12, False),
            ("population_messages", 10, False),
            ("message_counts", 80, False),
            ("message_delays", 80, False),
            ("building_list_burning_totals", 4, False),
            ("figure_sequence", 4, False),
            ("scenario_settings", 12, False),
            ("invasion_warnings", None, True),
            ("scenario_is_custom", 4, False),
            ("city_sounds", 8960, False),
            ("building_extra_highest_id", 4, False),
            ("figure_traders", 1604 + 100 * 2 * current_resource_total, False),
            ("building_list_burning", None, True),
            ("building_list_small", None, True),
            ("building_list_large", None, True),
            ("tutorial_part1", 32, False),
            ("enemy_army_totals", 20, False),
            ("building_storages", None, False),
            ("tutorial_part2", 4, False),
            ("gladiator_revolt", 16, False),
            ("trade_routes", None, True),
        ]
        for name, size, compressed in after_scenario:
            self.read_piece(name, size, compressed)

        self.empire = self.parse_empire_cities(self.pieces["empire_cities"])
        self.routes = self.parse_trade_routes(self.pieces["trade_routes"])
        self.buildings = self.parse_buildings(self.pieces["buildings"])
        self.building_resources = self.parse_building_resource_state(self.pieces["building_resource_state"])
        self.storages = self.parse_storage_state(self.pieces["building_storages"])
        self.demand_changes = self.parse_demand_changes(self.pieces["demand_changes"])


def format_resources(mapping, parser):
    return ", ".join(
        f"{parser.resource_name(slot)}={value}" if not isinstance(value, dict) else
        f"{parser.resource_name(slot)}={value.get('traded', '?')}/{value.get('limit', '?')}"
        for slot, value in sorted(mapping.items())
    ) or "-"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("save")
    ap.add_argument("--repo", default=".")
    args = ap.parse_args()

    parser = SaveParser(args.save)
    parser.load_resources_from_xml(args.repo)
    parser.parse()

    interesting_slots = [
        parser.resource_text_to_slot.get("marble"),
        parser.resource_text_to_slot.get("stone"),
        parser.resource_text_to_slot.get("sand"),
        parser.resource_text_to_slot.get("clay"),
        parser.resource_text_to_slot.get("timber"),
        parser.resource_text_to_slot.get("wheat"),
    ]
    interesting_slots = [slot for slot in interesting_slots if slot is not None]

    print(f"save={parser.path}")
    print(f"save_version={parser.save_version} resource_version={parser.resource_version}")
    print("resource save table:")
    for save_id in sorted(parser.resource_table):
        text = parser.resource_table[save_id]
        slot = parser.resource_text_to_slot.get(text)
        special = " special" if slot in parser.resource_special else ""
        print(f"  save_id={save_id:2d} text={text:<12} runtime_slot={slot}{special}")
    missing_specials = [
        name for name in ("denarii", "troops")
        if name not in parser.resource_table.values() and parser.resource_text_to_slot.get(name) in parser.resource_special
    ]
    print(f"resource_total_mapped={parser.resource_total_mapped}")
    print(f"specials_not_in_save_table={', '.join(missing_specials) or '-'}")
    print(f"trade_route_stored_resource_count={getattr(parser, 'trade_route_resource_count', 0)}")
    relevant_demand_changes = [
        change for change in parser.demand_changes["changes"]
        if change["resource"] in interesting_slots
    ]
    print(f"demand_changes={parser.demand_changes['count']} active_interesting={len(relevant_demand_changes)}")
    for change in relevant_demand_changes:
        print(
            f"  demand_change id={change['id']} route={change['route_id']} "
            f"resource={parser.resource_name(change['resource'])} amount={change['amount']} "
            f"buys={change['buys']} year={change['year']} month={change['month']}"
        )

    print("\nempire cities:")
    print(f"  record_size={parser.empire['record_size']} resources_to_load={parser.empire['resources_to_load']}")
    for city in parser.empire["cities"]:
        if city["type"] != 2 and not city["is_sea_trade"]:
            continue
        sells = {slot: city["sells"].get(slot) for slot in interesting_slots if city["sells"].get(slot)}
        buys = {slot: city["buys"].get(slot) for slot in interesting_slots if city["buys"].get(slot)}
        if sells or buys or city["is_sea_trade"]:
            print(
                f"  city={city['id']} type={city['type']} name_id={city['name_id']} "
                f"route={city['route_id']} open={city['is_open']} sea={city['is_sea_trade']} "
                f"delay={city['trader_entry_delay']} traders={city['trader_figure_ids']}"
            )
            print(f"    sells: {format_resources(sells, parser)}")
            print(f"    buys : {format_resources(buys, parser)}")
            route = parser.routes[city["route_id"]] if city["route_id"] < len(parser.routes) else None
            if route:
                route_sells = {slot: route["sells"].get(slot) for slot in interesting_slots if route["sells"].get(slot)}
                route_buys = {slot: route["buys"].get(slot) for slot in interesting_slots if route["buys"].get(slot)}
                print(f"    route sells traded/limit: {format_resources(route_sells, parser)}")
                print(f"    route buys  traded/limit: {format_resources(route_buys, parser)}")

    print("\ndocks and storage-adjacent buildings:")
    bres = parser.building_resources["entries"]
    for building_id, building in sorted(parser.buildings["buildings"].items()):
        type_id = building["type"]
        resource_entry = bres.get(building_id, {})
        accepted = resource_entry.get("accepted", {})
        resources = resource_entry.get("resources", {})
        accepted_interesting = {slot: accepted.get(slot) for slot in interesting_slots if accepted.get(slot)}
        resources_interesting = {slot: resources.get(slot) for slot in interesting_slots if resources.get(slot)}
        if type_id in {"dock", "warehouse", "warehouse_space", "granary"} or accepted_interesting or resources_interesting:
            print(f"  building={building_id} type={type_id} state={building['state']} saved_type={building['saved_type']}")
            if accepted_interesting:
                print(f"    accepted: {format_resources(accepted_interesting, parser)}")
            if resources_interesting:
                print(f"    resources: {format_resources(resources_interesting, parser)}")
            if resource_entry.get("warehouse_resource"):
                print(f"    warehouse_resource: {parser.resource_name(resource_entry['warehouse_resource'])}")

    print("\nstorages:")
    for storage in parser.storages["storages"]:
        interesting = {}
        for slot in interesting_slots:
            info = storage["resources"].get(slot)
            if info and (info["state"] != 1 or info["quantity"] != 3):
                interesting[slot] = info
        if interesting:
            rendered = ", ".join(
                f"{parser.resource_name(slot)}=state{info['state']}/qty{info['quantity']}"
                for slot, info in sorted(interesting.items())
            )
            print(f"  storage={storage['id']} building={storage['building_id']} permissions={storage['permissions']} {rendered}")


if __name__ == "__main__":
    main()
