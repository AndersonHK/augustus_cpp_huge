# Hippodrome Racing And Betting

Hippodrome racing is an optional BuildingType module rather than a hard-coded four-team feature. The effective mod stack owns the participant count, route, timing, graphics, and betting policy:

- Julius declares two teams without betting.
- Augustus overrides the module with two teams and betting.
- Vespasian overrides it with four teams and betting.

Any BuildingType may declare the module. The runtime spawns its configured FigureType participants, maintains an independent session for each venue, and opens the module's declared betting window when betting is enabled.

## Historical Problem

The Caesar 3 race animation created two horses. Augustus added a four-team betting window, but its visual race and financial result were not one model: a horse reaching the end invoked an unrelated random winner roll from one through four. Only two racers appeared, unused team colors remained in the UI, and the displayed finish could disagree with the payout.

The current implementation removes that independent result roll. The first actual participant to finish is the authoritative winner. Duplicate finish callbacks are idempotent, and settlement happens once against a complete configured field.

## Data Contract

The inline `<race>` module declares:

- participant FigureType and worker-scaled start-delay bands;
- lap count, ready period, minimum/maximum speed, and the randomized speed-roll interval;
- a cyclic building-local route, spawn point, and finish point;
- one or more stable team IDs with unique lanes, translations, directional body/vehicle graphics, placement offsets, layer order, and optional portraits;
- optional betting economics and a declarative UI window ID.

The spawn point must equal the route's first waypoint. The finish point must be one step behind that waypoint along the first route segment. These checks keep the explicit semantic points consistent with the cyclic route used by the orientation-aware legacy-compatible path.

Startup resolves the participant FigureType and every one of the eight directional body and vehicle entries. Betting races additionally resolve every portrait. Missing directions therefore fail before play instead of producing an invisible or partially composed racer.

## Runtime And Betting

`BuildingEntertainment::create_race_participants()` allocates the complete configured field atomically. Allocation failure removes the partial field and does not start or announce a race. Each participant carries its zero-based team index in the existing figure resource field.

At each waypoint a racer normally uses the configured minimum speed and has a small serialized-game-RNG chance to use the maximum speed. All racers share the declared route and lap count, so the variation affects real finish order without inventing a separate result.

`RaceSession` separates configured team capacity from the participant count recovered from a save. This preserves stable team indices when an old or inconsistent save contains only part of the field. A participant must register before it may register a finish. A complete current field settles its pending bet from the first recorded finish; a historical partial field cannot incorrectly settle a wager against absent teams.

The existing economics remain unchanged:

- a normal win adds the configured normal multiple of the wager;
- a festival win adds the configured festival multiple;
- a loss subtracts the wager without allowing negative savings.

Pending bet fields remain in the established city save packet. Active sessions and finish order are reconstructed runtime state. Stable mod-owned string IDs for saved team and venue ownership belong to the planned mod migration-ledger extension; until that format exists, an incomplete historical field leaves its wager pending for the next complete field rather than guessing.

## Declarative Window

The betting window is loaded from layered `UI/windows/*.xml`. XML owns widget type, layout, repeat source, conditions, bindings, tooltips, and whitelisted actions. Its C++ controller owns only race state and callbacks. The same window repeats over two Augustus teams or four Vespasian teams without fixed team arrays or coordinates in controller code. Julius does not expose it because its module disables betting.

The layout deliberately matches the established Augustus window contract: a 480x400 panel, five-pixel portrait inset inside 81x91 small-image borders, the original amount-panel and confirmation-button coordinates, and the original 24x24 close control. Button text retains the original vertical baseline offset. The savings translation already owns its punctuation, so the controller adds spacing rather than another colon. Legacy amount arrows previously addressed absolute atlas images 15-18; the Julius extractor now publishes those normal and pressed states as `UI\Arrow_Button`, allowing XML to select them without raw IDs or native draw code.

## Vespasian Graphics And Pipeline

Vespasian currently keeps four logical teams but temporarily renders them from the two inherited Julius horse/cart sets. Blue and red use their matching legacy sets; white and green use the opposite horse/cart pairings so all four configured racers remain independently addressable without shipping art that has not cleared review.

The withdrawn prototype remains preserved byte-for-byte in the separate `VespasianPixelArt` workspace. It contains four distinct two-layer horse-and-chariot assets, high-resolution sources, provenance, editable scenes, portable Python, local models, references, and intermediates. None of those provisional runtime sprites is duplicated under `Mods/Vespasian/Graphics` while the art direction is unresolved.

The deterministic pipeline enforces one horse, one chariot/driver, two wheels, eight mathematical isometric headings, clean binary alpha, shared per-direction transforms, union crops across animation frames, and analytically derived horse/cart offsets. Each shipped layer retains a 78x78 raster while its asset entry declares a 13x13 logical footprint; Julius and Augustus omit logical dimensions and retain their 1:1 vanilla presentation. It does not use image-generated frames, preventing inconsistent anatomy, view angles, halos, and disconnected compositions.

The prototype pass bakes a compact shadow by projecting each component silhouette down-right from one upper-left directional light. Validation requires minimum shadow area and width, grounding, directional reach, a one-pixel transparent perimeter, and logical-scale horse/cart continuity; alpha-valid circles or bars are not accepted. The deliberately simple 3D models remain artistically provisional and less detailed than the approved painted south-facing concept. Future work can improve source geometry, materials, silhouettes, harness, cloth, driver, and chariot detail without weakening the deterministic render/composition contract. Attractive generated concepts may guide design, but must not become independently generated directional frames whose topology cannot be guaranteed.

## Validation Contract

The startup harness validates session identity, unique participants, actual finish order, duplicate callbacks, partial restored fields, complete registry resolution, and every FigureType's live/dead graphics presentation. The executable gate loads Julius alone, Julius plus Augustus, and Vespasian, then round-trips and renders the required `.sav` and `.svv` set for 3,000 ticks at 1000% speed. Migration warnings are allowed only for repairs that disappear on immediate current-version reload; unresolved graphics, renderer fallbacks, new soak warnings, and errors fail the gate.

Manual validation remains necessary for the visible race in each mod stack, betting outcomes, supported window sizes/translations, all city orientations, and the final artistic bar.
