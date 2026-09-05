from __future__ import annotations

import json
import os
import sys
from collections import Counter
from pathlib import Path


def default_dump_path() -> Path:
    appdata = os.environ.get("APPDATA")
    if appdata:
        return Path(appdata) / "augustus" / "Vespasian" / "vespasian-runtime-crash-dump.json"
    return Path("vespasian-runtime-crash-dump.json")


def load_dump(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def attr_counts(items: list[dict], key: str) -> str:
    counts = Counter(item.get(key) or "<missing>" for item in items)
    return ", ".join(f"{name}={count}" for name, count in counts.most_common(12))


def summarize_buildings(buildings: list[dict]) -> None:
    live = [building for building in buildings if building.get("state")]
    houses = [building for building in live if building.get("house", {}).get("has_module")]
    rubble = [building for building in live if building.get("rubble", {}).get("is_rubble")]
    missing_type = [building for building in live if not building.get("type_attr")]
    mismatched_slots = [
        building for building in live
        if building.get("slot") != building.get("building_id") or
        building.get("building_id") != building.get("record_id")
    ]

    print(f"Buildings: {len(buildings)} runtime entries, {len(live)} live")
    print(f"  Houses: {len(houses)}")
    print(f"  Rubble: {len(rubble)}")
    print(f"  Missing type attrs: {len(missing_type)}")
    print(f"  Slot/id mismatches: {len(mismatched_slots)}")
    print(f"  Top types: {attr_counts(live, 'type_attr')}")

    for building in missing_type[:20]:
        print(
            "  missing-type building "
            f"id={building.get('building_id')} type_id={building.get('type_id')} "
            f"state={building.get('state')} grid={building.get('grid_offset')}"
        )

    for building in mismatched_slots[:20]:
        print(
            "  id-mismatch building "
            f"slot={building.get('slot')} building_id={building.get('building_id')} "
            f"record_id={building.get('record_id')} type={building.get('type_attr')}"
        )


def summarize_figures(figures: list[dict], figure_runtime: list[dict]) -> None:
    live = [figure for figure in figures if figure.get("state")]
    missing_type = [figure for figure in live if not figure.get("type_attr")]
    unknown_refs: list[tuple[int, str, str | None]] = []
    for figure in live:
        for name in ("building", "immigrant_building", "destination_building"):
            ref = figure.get(name) or {}
            pointer = ref.get("pointer")
            known_id = ref.get("known_building_id")
            if pointer and pointer != "0000000000000000" and not known_id:
                unknown_refs.append((figure.get("id"), name, pointer))

    stale_runtime = [
        entry for entry in figure_runtime
        if entry.get("figure_pointer") and not entry.get("known_figure_id")
    ]

    print(f"Figures: {len(figures)} live entries")
    print(f"  Missing type attrs: {len(missing_type)}")
    print(f"  Unknown building refs: {len(unknown_refs)}")
    print(f"  Stale native runtime entries: {len(stale_runtime)}")
    print(f"  Top types: {attr_counts(live, 'type_attr')}")

    for figure in missing_type[:20]:
        print(
            "  missing-type figure "
            f"id={figure.get('id')} type_id={figure.get('type_id')} "
            f"state={figure.get('state')} action={figure.get('action_state')}"
        )

    for figure_id, ref_name, pointer in unknown_refs[:30]:
        print(f"  unknown-building-ref figure={figure_id} ref={ref_name} pointer={pointer}")

    for entry in stale_runtime[:20]:
        print(
            "  stale-runtime-entry "
            f"slot={entry.get('slot')} pointer={entry.get('figure_pointer')} "
            f"definition={entry.get('definition_attr')} profile={entry.get('profile_id')}"
        )


def summarize_population(population: dict) -> None:
    if not population:
        return

    ages = population.get("at_age") or []
    print("Population:")
    print(
        "  "
        f"population={population.get('population')} "
        f"capacity={population.get('total_capacity')} "
        f"room={population.get('room_in_houses')} "
        f"yearly_update_requested={population.get('yearly_update_requested')} "
        f"yearly_deaths={population.get('yearly_deaths')}"
    )
    print(
        "  "
        f"last_used_house_add={population.get('last_used_house_add')} "
        f"last_used_house_remove={population.get('last_used_house_remove')}"
    )
    if len(ages) == 100:
        decennia = [sum(ages[index:index + 10]) for index in range(0, 100, 10)]
        print("  Decennia: " + ", ".join(f"{index // 10}={value}" for index, value in enumerate(decennia)))


def main() -> int:
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else default_dump_path()
    dump = load_dump(path)

    metadata = dump.get("metadata", {})
    print(f"Runtime dump: {path}")
    print(
        "Metadata: "
        f"reason={metadata.get('reason')} "
        f"year={metadata.get('game_year')} "
        f"month={metadata.get('game_month')} "
        f"day={metadata.get('game_day')} "
        f"tick={metadata.get('game_tick')}"
    )

    summarize_buildings(dump.get("buildings") or [])
    summarize_figures(dump.get("figures") or [], dump.get("figure_runtime") or [])
    summarize_population(dump.get("city_population") or {})
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
