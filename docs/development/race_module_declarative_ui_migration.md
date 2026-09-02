# Race module and declarative UI migration checklist

Status: current race slice implemented and validated; the betting-window parity correction is automated, while manual validation, replacement art, and the explicitly deferred adjacent UI/attractions architecture remain.

## Behavioral contracts

- [x] Julius hippodrome declares exactly two teams and exposes no betting UI or Augustus-only assets.
- [x] Augustus hippodrome declares exactly two teams and exposes betting.
- [x] Vespasian hippodrome declares exactly four teams and exposes betting.
- [x] Any BuildingType with a valid inline race module can host one-to-N racers without a hard-coded building-type check.
- [x] Race results come only from actual finish order; bets settle once against that order.
- [x] Multiple race-enabled buildings use independent race sessions.

## Declarative UI runtime

- [x] Enumerate and layer every `UI/windows/*.xml` file instead of hard-coding the main menu and mission briefing paths.
- [ ] Add strict typed bindings for text, translations, integers, money, booleans, and asset references.
- [x] Add declarative visibility, enabled, and selected conditions.
- [ ] Add nested containers and list-backed repeaters with scoped item bindings and an item index.
- [ ] Add generic draw, focus, hit-test, tooltip, and action dispatch through a whitelisted controller interface.
- [ ] Reject unknown widget types, attributes, bindings, actions, invalid repeater templates, and unresolved images during startup.
- [ ] Convert mission briefing to the shared interpreter as the compatibility proof; keep only its data provider and callbacks.
- [x] Convert the race/betting window so C++ owns only state and callbacks.
- [x] Restore the established 480x400 betting layout, portrait insets, small-image borders, extracted arrow/close controls, centered button baselines, and money-label punctuation.
- [x] Extract legacy absolute arrow images into the named runtime group `UI\Arrow_Button`; XML contains no raw image IDs or draw fallback.

## BuildingType race module

- [x] Add an optional strict inline `<race>` schema to BuildingType.
- [x] Parse participant FigureType, worker-scaled start timing, route, lanes, laps, timing, speed variation, teams, two-layer placement, betting, and window references.
- [ ] Validate one-to-N unique teams, stable string IDs, lanes, route points, graphics, UI images, localization keys, and betting/window consistency.
- [x] Keep the schema inline for this slice; defer an `Attractions` registry/folder until Arena and Pharaoh festival requirements establish the correct shared boundary.
- [ ] Resolve race window and FigureType references only after UI, FigureType, and BuildingType registries are staged.

## Generic race runtime

- [x] Replace the global four-entry HippodromeRace controller with per-building race sessions.
- [x] Track configured capacity, expected/registered participants, deterministic saved-game RNG speed variation, finish order, and the optional persisted bet ticket.
- [x] Spawn all configured racers atomically from the owning building module.
- [ ] Move hippodrome route/action logic from `animal.cpp` into a generic race-participant runtime.
- [x] Interpret the participant team ordinal only through its owner's RaceModule definition.
- [x] Dispatch race participant generation for every race-enabled BuildingType before the building-specific service branch.
- [ ] Remove global `hippodrome_has_race` assumptions; provide per-building and city-wide race queries.
- [ ] Replace direct emperor `bet_amount` reservations with a generic reserved-personal-funds query.

## Save compatibility

- [x] Keep legacy Hippodrome FigureType/action IDs loadable while routing them through the data-owned participant runtime.
- [ ] Bridge legacy numeric chosen-horse and bet fields to a stable team string ID and owning race session.
- [x] Reconstruct active race sessions lazily after figure/building relationships are stable.
- [x] Warn for actual migration/repair, and produce a clean current-version round trip.
- [ ] Handle owner removal, participant removal, incomplete historical two-racer sessions, and duplicate finish callbacks without invariant violations.

## Graphics and building presentation

- [x] Move hard-coded hippodrome horse/cart entries, directional offsets, and layer order into team graphics data.
- [x] Move hard-coded spectator drawing and legacy group enums into BuildingType graphics conditions driven by race-active state, population, composition role, and orientation.
- [ ] Remove hippodrome banners, team arrays, betting buttons, and fixed coordinates from the building culture window.
- [ ] Select building and interaction window layouts from the RaceModule.
- [x] Keep Augustus-only UI asset paths out of Julius XML and generic C++.

## Vespasian-authored racers

- [ ] Ship an original, coherent Vespasian sprite family for all four teams, not a mix of Julius and newly authored racers.
- [ ] Raise the deliberately simple 3D source models to the original/Augustus artistic bar without weakening deterministic topology or composition validation.
- [x] Blue: youthful/promising Neptune association, expressed with very subtle marine trim.
- [x] Red: veteran former-oarsman/consular-savior association, expressed with very subtle naval/veteran trim.
- [x] White: former-gladiator association, expressed with very subtle arena/combat trim.
- [x] Green: Celeres/Kingdom-era cavalry association, expressed with very subtle archaic equestrian trim.
- [x] Preserve shared silhouette, scale, viewpoint, eight directions, animation cadence, cart offsets, and transparent background across all four teams.
- [x] Do not copy or recolor Caesar 3 pixels; use originals only as dimensional/motion references and retain Vespasian authorship/provenance.
- [x] Remove the artistically provisional 78x78 runtime package from `Mods/Vespasian/Graphics`; preserve its byte-identical reference package in the separate `VespasianPixelArt` workspace.
- [x] Keep full-resolution editable source layers, provenance, audit output, and local-model tooling in the separate `VespasianPixelArt` workspace; never ship a `Source` folder inside a mod's `Graphics` tree.
- [x] Replace the rejected image-generated sheets with deterministic geometry-rendered horse and cart/driver layers; no image-generated cell feeds a shipped derivative.
- [x] Enforce one single-horse topology, one two-wheel cart/driver topology, and eight mathematically fixed isometric headings with no extra heads, riders, or adjacent-cell contamination.
- [x] Use explicit binary alpha, clear RGB behind transparent pixels, preserve edge-on north/south wheel planes, and crop every layer without loss.
- [x] Bake silhouette-derived component shadows from one upper-left directional light and reject missing, token, ungrounded, non-directional, clipped, or fully occluded shadows.
- [x] Validate every team/direction/frame with zero unresolved native graphics requests.

### Asset-pipeline audit

- [x] Keep the portable Python runtime and local model stack scoped to the separate `VespasianPixelArt` workspace beneath the GitHub projects directory; install nothing system-wide.
- [x] Keep Quaternius's CC0 horse model, Blender scene generation, downloaded model weights, rejected experiments, and build intermediates outside this repository.
- [x] Record CC0 provenance beside the full-resolution source images in the separate `VespasianPixelArt` workspace.
- [x] Reject FLUX.2 Klein and SDXL ControlNet outputs for this asset because they did not preserve multi-view topology; keep the models only for future upscale research.
- [x] Scale the authored chariot about the fixed horse harness so both layers fit the shared 78x78 source canvas without independent geometric distortion.
- [x] Resize horse and cart with one common transform per direction; derive each vehicle offset from the two crop origins rather than fitting a single visual point.
- [x] Use one union horse crop per direction across all eight run frames to prevent animation jitter.
- [x] Validate all 288 runtime PNGs as 78x78, binary-alpha, transparent-RGB-clean, palette-bounded images; reject source-pixel loss and require a one-pixel transparent perimeter around the complete lit sprite.
- [x] Validate a meaningful silhouette-projected shadow below the contact plane in every runtime layer, including minimum area, width, directional reach, and animation continuity.
- [x] Validate all 256 team/direction/frame composites at the 13x13 logical presentation size: horse/cart mask distance is at most one logical pixel after filtered reduction.
- [x] Preserve the legacy renderer equation: horse and cart select the same direction suffix, while placement and layer order use `(direction + 4) % 8`.
- [x] Correct the legacy draw-behind mask to `ne,e,w,nw,n`; the prior data omitted `w`.
- [x] Independently audit offsets and layer semantics against installed extracted graphics without copying proprietary pixels into the repository.
- [x] Validate the promoted prototype; the maintainer confirmed sound structure and fluid animation before it was withdrawn from shipping for further art direction.
- [x] Split the four teams into `Entertainment\Hippodrome_Blue`, `_Red`, `_White`, and `_Green`; keep generic entry IDs inside each conceptual asset.
- [x] Preserve the accepted 25x25 geometry/rotation baseline as an explicitly incomplete external checkpoint, then preserve the higher-detail 78x78 directional-shadow pass separately.
- [x] Keep source and logical dimensions asset-owned: logical dimensions use 1/120-pixel fixed-point units, so a future 78x78 Vespasian entry presented at 13x13 pixels declares `width="78" height="78" logical_width="1560" logical_height="1560"`; Julius and Augustus normally omit logical dimensions and remain 1:1.
- [x] Validate source/logical size, animation-frame propagation, anchors, logical horse/cart offsets, and layer ordering in the startup harness.
- [x] Replace the centered lane offset with positive, distinct, data-declared inward distances and validate the configured margin/spacing contract at startup.
- [ ] Visually confirm in game that every composed racer remains clear of the crowd and median in all four city orientations.
- [ ] Perform the final in-game all-orientation art review after the higher-detail model pass.

## Mod data

- [x] Julius BuildingType declares two teams, no betting, and a no-bet building presentation.
- [x] Augustus BuildingType declares two teams, betting economics, and Augustus race UI assets.
- [x] Vespasian BuildingType declares four teams and betting; until replacement art clears review, its four logical teams temporarily compose the two inherited Julius horse/cart sets.
- [x] Let Vespasian inherit the Augustus betting window when no layout difference is required; team repeaters adapt from two to four items.
- [x] Do not add duplicate extracted graphics XML or PNG output to authored mod directories.

## Adjacent UI/data-owned slice

- [ ] Convert hippodrome building information to declarative widgets and race bindings.
- [ ] Convert the entertainment-advisor games section to declarative widgets/capability bindings.
- [ ] Convert `hold_games` to a repeated-option declarative transaction window.
- [ ] Move Colosseum/Hippodrome banner and construction presentation paths out of C++ and into mod-owned UI/BuildingType data.
- [ ] Keep broader trade/distribution/festival UI migrations documented as follow-up work unless the shared runtime requires them directly.

## Automated validation

- [ ] Parser fixtures: valid one-, two-, four-, and larger-N races.
- [ ] Negative fixtures: duplicate teams/lanes, missing route, invalid odds, unresolved FigureType/window/graphics/localization, betting mismatch, unknown XML.
- [ ] UI fixtures: two- and four-team repeaters, selection, enabled/disabled wager controls, tooltips, long localization, and action arguments.
- [ ] Runtime fixtures: atomic spawn, synchronized start, seeded variation, finish ordering, duplicate callbacks, participant/owner deletion, multiple venues, and mid-race save/load.
- [x] Julius-only startup and 3,000-tick save soak.
- [x] Julius+Augustus startup and 3,000-tick save soak.
- [x] Vespasian complete required/representative save suite for 3,000 ticks at 1000% speed.
- [x] Confirm current-version round trips have no repeated migration warning.
- [x] Confirm steady-state throughput reaches at least 1,000 TPS by the third second.
- [x] Deploy the exact validated Release binary and authored mod data without replacing installed extracted graphics.
- [x] Delete and freshly regenerate installed Julius and Augustus runtime graphics from the Caesar 3 SG2 files and shipped Augustus atlases before the final gate.

## Manual validation

- [ ] Julius: observe two racers and no betting affordance.
- [ ] Augustus: observe two racers, place bets on both, and match settlement to visible finish order.
- [ ] Vespasian: confirm the temporary four-team/two-legacy-set mapping and later review four coherent, subtly distinct replacement racers in every hippodrome orientation.
- [ ] Validate race/building windows at supported resolutions, UI scales, and representative long translations.
- [ ] Validate normal win, festival win, loss, insufficient funds, race-in-progress lock, and old pending-bet carry-forward.

## Deferred architecture

- [ ] Design an `Attractions` definition registry/folder after Race, Arena, and Pharaoh festival requirements are understood.
- [ ] Decide whether routes, participants, betting, crowd presentation, and festival scheduling become separate reusable attraction submodules.
