# Hippodrome race and betting implementation checklist

Status: implementation, extraction validation, deployment, and automated validation complete; manual validation remains.

## Crash prerequisite

- [x] Read the latest Vespasian log and crash report.
- [x] Identify the crashing save as `Praetor 2 9.svv` and the first-frame failure as a prefect with `num_attackers=2` but empty opponent/attacker relationships.
- [x] Trace the corruption to one-pass figure loading: forward relationships were attached before later figure slots were reset, and `EndpointReassigned` disconnected them without changing the already-loaded combat counters.
- [x] Stage all figure relationship ids and attach them only after every figure record is stable.
- [x] Validate targets and normalize the loaded combat tuple; warn only for a repair and write a clean current-version round trip.
- [x] Reproduce the original save at 3,000 ticks without a crash or repeated migration warning.

## Four-team race

- [x] Spawn blue, red, white, and green participants atomically; remove a partial allocation instead of starting an incomplete new race.
- [x] Give all four racers the same start delay, six-lap route contract, base speed, and slight serialized-RNG checkpoint variation.
- [x] Keep four visible sub-lanes and four distinct horse/cart combinations using the inherited Julius bridge assets.
- [x] Do not add extracted sprites, generated graphics, or duplicate asset XML.
- [x] Record one finish position per team and treat the first actual crossing as the winner.
- [x] Remove the independent standard-library random result.
- [x] Settle a pending bet exactly once from the actual winner while preserving current normal/festival economics.
- [x] Lock betting while a race is active and recheck the lock when the confirmation button executes.
- [x] Require all four teams to register before settlement. A loaded historical two-racer race leaves a wager pending for the next complete race.
- [x] Reset non-persisted race runtime on scenario initialization and load without clearing the saved pending bet.

## Automated validation

- [x] Add controller tests for invalid ids, invalid teams, unique participation, duplicate callbacks, actual order, winner lookup, and new-race reset.
- [x] Run Julius-only startup and a 3,000-tick save soak.
- [x] Run the Julius plus Augustus dependency stack startup and a 3,000-tick save soak.
- [x] Run Vespasian startup and the complete required/representative `.sav` and `.svv` suite for 3,000 ticks at 1000% speed on the pre-deployment implementation.
- [x] Confirm migration repairs disappear on the current-version round trip.
- [x] Confirm the completed saves exceed 1,000 steady-state TPS.
- [x] Rebuild Release after the final complete-field settlement guard.
- [x] Deploy the Release executable and authored mod data.
- [x] Validate fresh Julius and Augustus extraction products from the game files/install assets in the ignored validation directory, with no extraction into the repository.
- [x] Repeat startup, the original crashing save, and the final required save gate against the exact final deployed binary.

## Manual validation for the user

- [ ] Observe four simultaneous racers in each hippodrome orientation and confirm their sub-lanes/depth are readable.
- [ ] Confirm each team is distinguishable from its horse/cart combination; decide separately whether shader/palette-owned faction liveries are desirable.
- [ ] Bet on every team across multiple races and confirm the warning and savings change match the visibly first racer.
- [ ] Confirm normal, festival, loss, and insufficient-savings outcomes.
- [ ] Open the betting window immediately before a race and confirm it cannot place a wager after the race starts.
- [ ] Load an old active two-racer race with a pending white/green bet and confirm the bet carries into the next complete four-team race.

## Deferred enhancements, not blockers

- [ ] Persist and display the previous race's full standings if the hippodrome UI gains a race-history panel.
- [ ] Add a renderer-owned palette-remap/livery parameter that maps naturally to the planned Vulkan atlas/shader path; do not author recolored extracted PNGs.
