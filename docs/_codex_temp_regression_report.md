# Temporary Regression Report

Delete this file once all listed regressions are fixed and reflected in the normal progress tracker.

## Actor Colony / Theater Regression

- Save: `Procurator Attempt 1 13 - mid refactor.svv`.
- Actor colonies remain broken in this save.
- Newly placed theaters and actor colonies also fail in this save.
- Other saves have working actor colonies and gladiator schools, so the failure may be save-state, routing, profile hydration, venue state, or city registry specific rather than globally broken XML.
- Candidate fix: Vespasian entertainment seeker/service FigureType pathing now allows `roads_highway`, venue seeker selection no longer rejects venues solely because the save's cached `distance_from_entry` is zero, and route planning plus per-tile movement now ask the same `PathingMode` roadblock rule so `venue_seeker` ignores roadblocks while entertainment service walkers keep their normal permission behavior.

## Timber Yard / Distribution Regression

- Save: `Engineer attempt 1 27 - resaved mid refactor`.
- Timber yards report they cannot export output while furniture workshops report lacking timber.
- Freshly rebuilt timber yards work in `Engineer attempt 1 27 - resaved mid refactor with timber yards rebuilt`.
- Suspect cart pusher, warehouse/distribution reservations, save-load hydration, or producer output route state.
- Candidate fix: save-load restores omitted native storage resources from flat record mirrors. Native producer spawn now runs through `building_runtime`, reserves `Building::reserve_output_storage_loads()` output in addition to legacy `has_fish` figure-delivery buffers, and deletes the dead wharf/shipyard legacy spawn branches.

## Entertainment Overlay Animation Regression

- Theaters do not animate while the entertainment overlay is active.
- Candidate fix: entertainment overlays now defer animation visibility to the active overlay's `show_building(...)` result instead of suppressing all theater-family animations.

## Vespasian Amphitheater Animation Regression

- Vespasian amphitheaters animate even when neither gladiators nor actors are present.
- Both show 1 and show 2 report not happening, so animation gating should require active entertainment content.
- Candidate fix: Vespasian amphitheater default and upgrade-off graphics are marked non-animated; animated variants still require positive show days.
