#!/usr/bin/env python3
"""Read the uncompressed terrain ledger in native SVV 207 / scenario 27 archives."""
import argparse
import json
from pathlib import Path
import re
import sys


def inspect(path):
    matches = re.findall(rb"terrain-ledger\t1\n.*?end-terrain-ledger\n", path.read_bytes(), re.DOTALL)
    if len(matches) != 1:
        raise ValueError("Expected one native terrain ledger; older saves use the legacy bridge instead")
    definitions, sets = {}, {}
    for row in matches[0].decode("utf-8").splitlines()[1:-1]:
        fields = row.split("\t")
        if fields[0] == "terrain" and len(fields) == 3:
            identity = int(fields[1])
            if identity <= 0 or identity in definitions:
                raise ValueError("Invalid or duplicate terrain identity")
            definitions[identity] = fields[2]
        elif fields[0] == "set" and len(fields) >= 2:
            identity = int(fields[1])
            if identity < 0 or identity in sets:
                raise ValueError("Invalid or duplicate terrain-set identity")
            sets[identity] = [definitions[int(reference)] for reference in fields[2:]]
        else:
            raise ValueError("Malformed terrain ledger record")
    if sets.get(0) != []:
        raise ValueError("Terrain set zero must be empty")
    return {"terrains": definitions, "sets": sets}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.archive), ensure_ascii=False, indent=2))
        return 0
    except (OSError, ValueError, KeyError, UnicodeError) as error:
        print(f"Terrain inspection failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
