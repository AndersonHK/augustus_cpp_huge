#!/usr/bin/env python3
import argparse
import struct
import sys
import zlib
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

# Local copy of the parser used for save inspection.
from tools.inspect_save_trade import SaveParser

UNCOMPRESSED = 0x80000000


def rebuild_routes_for_trade_cities(path):
    parser = SaveParser(str(path))
    parser.load_resources_from_xml(str(Path(__file__).resolve().parents[1]))
    parser.parse()

    if 'trade_routes' not in parser.pieces or not parser.piece_offsets.get('trade_routes'):
        raise RuntimeError('trade_routes chunk not found in save')

    if not parser.routes:
        raise RuntimeError('no trade routes parsed from save')

    route_resource_count = getattr(parser, 'trade_route_resource_count', 0)
    if route_resource_count <= 0:
        raise RuntimeError('invalid trade route resource count')

    # Build per-route resource buckets directly from the decoded trade-route payload.
    # save_id space includes all ids stored in the file; some ids may be unmapped.
    raw = parser.pieces['trade_routes']
    if len(raw) < 4:
        raise RuntimeError('trade_routes payload too small')
    route_count = struct.unpack_from('<i', raw, 0)[0]

    # The route payload layout is:
    # [route_count][for each route for each side(buy/sell) for each save id: limit, traded]
    # where side==0 is sells and side==1 is buys in legacy serialization.
    class Buf:
        def __init__(self, data):
            self.data = data
            self.i = 0

        def i32(self):
            v = struct.unpack_from('<i', self.data, self.i)[0]
            self.i += 4
            return v

        def write_i32(self, out, value):
            out.extend(struct.pack('<i', int(value)))

    route_payload = Buf(raw)
    route_payload.i += 4  # skip route_count

    parsed = []
    for _ in range(route_count):
        sells = []
        buys = []
        for _ in range(route_resource_count):
            sells.append({'limit': route_payload.i32(), 'traded': route_payload.i32()})
        for _ in range(route_resource_count):
            buys.append({'limit': route_payload.i32(), 'traded': route_payload.i32()})
        parsed.append({'sells': sells, 'buys': buys})

    # Map trade cities to route ids and remembered flags from empire city table.
    route_flags = {}
    for city in parser.empire['cities']:
        if not city['in_use'] or city['route_id'] >= route_count:
            continue
        city_type = city['type']
        # 2 is EMPIRE_CITY_TRADE in this savefile lineage.
        if city_type != 2:
            continue
        flags = route_flags.setdefault(city['route_id'], {'sells': set(), 'buys': set()})
        for save_id in range(route_resource_count):
            if save_id == 0:
                continue
            slot = parser.save_id_to_slot(save_id)
            if slot is None:
                continue
            if slot in city['sells']:
                flags['sells'].add(save_id)
            if slot in city['buys']:
                flags['buys'].add(save_id)

    changed = 0
    for route_id in range(route_count):
        flags = route_flags.get(route_id)
        if not flags:
            # Keep unrelated routes as-is.
            continue

        sells = parsed[route_id]['sells']
        buys = parsed[route_id]['buys']
        for save_id in range(route_resource_count):
            if save_id == 0:
                continue
            want_sell = save_id in flags['sells']
            want_buy = save_id in flags['buys']

            if want_sell:
                if sells[save_id]['limit'] <= 0:
                    sells[save_id]['limit'] = 25
                    changed += 1
            else:
                sells[save_id]['limit'] = 0

            if want_buy:
                if buys[save_id]['limit'] <= 0:
                    buys[save_id]['limit'] = 25
                    changed += 1
            else:
                buys[save_id]['limit'] = 0

            # Keep traded counts only when the resource remains allowed on that side.
            if not want_sell:
                sells[save_id]['traded'] = 0
            elif sells[save_id]['traded'] > sells[save_id]['limit']:
                sells[save_id]['traded'] = sells[save_id]['limit']

            if not want_buy:
                buys[save_id]['traded'] = 0
            elif buys[save_id]['traded'] > buys[save_id]['limit']:
                buys[save_id]['traded'] = buys[save_id]['limit']

    rebuilt = bytearray()
    rebuilt.extend(struct.pack('<i', route_count))
    for route in parsed:
        for bucket in (route['sells'], route['buys']):
            for entry in bucket:
                rebuilt.extend(struct.pack('<i', int(entry['limit'])))
                rebuilt.extend(struct.pack('<i', int(entry['traded'])))

    piece_start, piece_end = parser.piece_offsets['trade_routes']
    data = bytearray(Path(path).read_bytes())
    chunk = bytearray()
    chunk.extend(struct.pack('<i', len(rebuilt)))
    chunk.extend(struct.pack('<I', UNCOMPRESSED))
    chunk.extend(rebuilt)

    new_data = data[:piece_start] + chunk + data[piece_end:]
    if new_data == data:
        return False, changed, route_count

    Path(path).write_bytes(new_data)
    return True, changed, route_count


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('save', help='Path to Quaestor attempt save file')
    args = ap.parse_args()

    path = Path(args.save)
    if not path.is_file():
        raise SystemExit(f'File not found: {path}')

    changed, total, routes = rebuild_routes_for_trade_cities(path)
    print(f'patched={changed} changed_entries={total} routes={routes}')


if __name__ == '__main__':
    main()
