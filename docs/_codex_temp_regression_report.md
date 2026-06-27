# Temporary Regression Report

Delete this file once all listed regressions are fixed and reflected in the normal progress tracker.

## Status Checkpoint

- [x] Actor colony / theater roadblock regression resolved in later runtime testing.
- [x] Timber yard / raw producer stale cart output regression resolved in later runtime testing.
- [x] Entertainment overlay animation regression resolved in later runtime testing.
- [x] Vespasian amphitheater no-show animation gate has a candidate fix and has not reappeared in recent reports.
- [ ] Garden area placement still needs a full rewrite; current fixes are bridge fixes.
- [ ] Concrete carts with no destination still need explicit idle-at-source behavior if not already covered by the current cart wait action.

## Resolved: Actor Colony / Theater Regression

- Save: `Procurator Attempt 1 13 - mid refactor.svv`.
- Actor colonies remain broken in this save.
- Newly placed theaters and actor colonies also fail in this save.
- Other saves have working actor colonies and gladiator schools, so the failure may be save-state, routing, profile hydration, venue state, or city registry specific rather than globally broken XML.
- Candidate fix: Vespasian entertainment seeker/service FigureType pathing now allows `roads_highway`, venue seeker selection no longer rejects venues solely because the save's cached `distance_from_entry` is zero, and route planning plus per-tile movement now ask the same `PathingMode` roadblock rule so `venue_seeker` ignores roadblocks while entertainment service walkers keep their normal permission behavior.
- Current status: no longer blocking after roadblock/pathing-mode consistency fixes; keep this note only until the next regression sweep confirms no save-specific actor spawn issue remains.

## Resolved: Timber Yard / Distribution Regression

- Save: `Engineer attempt 1 27 - resaved mid refactor`.
- Timber yards report they cannot export output while furniture workshops report lacking timber.
- Freshly rebuilt timber yards work in `Engineer attempt 1 27 - resaved mid refactor with timber yards rebuilt`.
- Suspect cart pusher, warehouse/distribution reservations, save-load hydration, or producer output route state.
- Candidate fix: save-load restores omitted native storage resources from flat record mirrors. Native producer spawn now runs through `building_runtime`, reserves `Building::reserve_output_storage_loads()` output in addition to legacy `has_fish` figure-delivery buffers, and deletes the dead wharf/shipyard legacy spawn branches.
- Current status: no longer blocking after raw material producer/output cart fixes.

## Resolved: Raw Material Producer Output Stall

- Several saves from the current refactor window can have raw material producers stuck at `100/100` output storage without spawning a cart pusher.
- Replacing the producer with a fresh building fixes the issue, so the strongest suspect is stale save/runtime bridge state, stale figure ids, or a producer-output reservation that was serialized or hydrated incorrectly.
- Keep this distinct from normal "no destination accepts output" behavior: these producers remain full and idle when the live city should be able to move the output after a rebuild.
- Candidate fix: `Figure::remove()` now clears owning, destination, and immigrant building figure slots by matching the removing figure id before type-specific cleanup, so owned carts cannot disappear while leaving their producer slot stale. Spawn guards now use alive/dead checks instead of truthy `state`, so `FIGURE_STATE_DEAD` no longer counts as a live blocking cart. Save-load also clears only producer primary output-cart slots that point to missing or dead figures; it does not clear live parented carts based on action state.
- Candidate fix: live producer carts can no longer retarget from warehouse overflow to workshop delivery while keeping the old warehouse destination. Cart destination validation now receives the intended action explicitly and has a workshop-delivery branch, preventing an action/destination split that could leave sand carts visually trying to deposit while holding the wrong delivery state.
- Current status: manually confirmed fixed with raw material output after the cart destination/action split fix.

## Resolved: Entertainment Overlay Animation Regression

- Theaters do not animate while the entertainment overlay is active.
- Candidate fix: entertainment overlays now defer animation visibility to the active overlay's `show_building(...)` result instead of suppressing all theater-family animations.
- Current status: manually confirmed theater rendering/animation behavior recovered in later testing.

## Mostly Resolved: Vespasian Amphitheater Animation Regression

- Vespasian amphitheaters animate even when neither gladiators nor actors are present.
- Both show 1 and show 2 report not happening, so animation gating should require active entertainment content.
- Candidate fix: Vespasian amphitheater default and upgrade-off graphics are marked non-animated; animated variants still require positive show days.
- Current status: not recently reported after the candidate fix; leave here until a focused Vespasian amphitheater retest confirms both no-show and one-show states.

## Open: Garden Area Placement Regression

- Non-blocking after the current checkpoint because clear-land fixes the corrupted tiles and saves remain usable.
- Garden area placement still has edge cases when a preview or placement overlaps existing committed gardens.
- Expanding/retracting a garden drag can still interact badly with 2x2 garden composition.
- Placing overgrown gardens over regular gardens can instantiate 1x1 garden runtime tiles on top of existing 2x2 garden tiles.
- Required fix: rewrite garden area placement from the ground up so preview and commit operate on an isolated placement model, then atomically refresh committed garden tiles. Preview must never be allowed to re-anchor, peel, or partially overwrite existing 2x2 garden composition.
- First guard was too strict and blocked normal drag extension over existing gardens. Current targeted fix allows existing garden terrain inside the drag rectangle again, clears the previous preview's garden runtime bindings narrowly, and globally recomposes gardens after preview restore/placement as a bridge; full atomic garden replacement remains open.

## Open: Concrete Cart No-Destination Regression

- Cart pushers carrying concrete can animate as walking-in-place for a while and eventually despawn when there is no destination for concrete.
- Concrete cannot be stored in warehouses, so a city with no active construction sites can legitimately have zero valid concrete inputs.
- Expected behavior: a cart with no destination should remain alive at the source on its idle standing frame, periodically recheck destinations, and start moving once a construction-site input opens. Do not add warehouse fallback behavior for concrete.
- Candidate fix: `FIGURE_ACTION_245_CARTPUSHER_WAITING_FOR_DESTINATION` no longer kills the cart after a max wait, rechecks destinations periodically, and pins `image_offset` to the standing frame while waiting.
