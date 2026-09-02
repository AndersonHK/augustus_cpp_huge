# Hippodrome race and betting fix plan

Status: researched and planned; implementation has not started.

## Target behavior

- Augustus keeps the four historical teams: blue, red, white, and green.
- Every team offered in the betting window is visibly represented in the race.
- The displayed winner and the betting winner are the same team.
- Each team has an unbiased 25% chance to win, preserving the documented odds.
- A bet is attached to one race, settled exactly once, and survives save/load without rerolling.
- Julius retains its base Caesar 3 race presentation unless its own definitions explicitly opt into Augustus betting.

## Research record

- The base two-racer hippodrome animation predates Augustus betting. It uses two routes and the Caesar 3 walker groups now extracted as `Walkers\Group_217` through `Walkers\Group_220`.
- Augustus PR [#409](https://github.com/Keriew/augustus/pull/409), merged as [`40b8040e`](https://github.com/Keriew/augustus/commit/40b8040e5ffa5cb5ab0e17a57cff4142bffc9190) on 22 July 2021, added four betting choices and a four-way random result. It did not add racers, ranking, white/green track graphics, or a race controller.
- The PR description says the notification reports whether the selected horse won, but the implementation calls `race_result_process()` from a finishing visual horse and then generates an unrelated random number from 1 through 4.
- Follow-up commits added [tooltips](https://github.com/Keriew/augustus/commit/9779583e1e1760e4f8545c94f38b73866753d8d7), [team descriptions](https://github.com/Keriew/augustus/commit/ec837f347ae8a898b13feab1ab699f5e3da0a6d6), and [window improvements](https://github.com/Keriew/augustus/commit/5a51cd5da4a1877e3963a79a890f945146b07251), but did not change settlement semantics.
- The Augustus 3.1/4.0 documentation describes four historical teams, a 25% chance, and a doubled normal payout. That confirms the four-team UI and equal odds are intentional even though the track was never integrated with them.
- Public issue [#1175](https://github.com/Keriew/augustus/issues/1175) reports that the transient result notification is easy to miss and asks for the previous bet/result in the hippodrome window. It was closed without a linked code change.
- PR [#1363](https://github.com/Keriew/augustus/pull/1363) only lengthened the two routes so racers stop clipping through decorations. It did not increase movement speed; it slightly lengthened the course and adjusted the route checkpoint count from 22 to 24.
- The extracted Augustus package has four team portraits in `UI\Hipp_Team_Blue`, but no white/green walker animation groups. The racing horse/cart frames are inherited from the Caesar 3 extraction through Julius.
- No public Augustus issue specifically reporting the four-team/visible-race/result disconnect was found.

## Root cause

Betting was added as a UI and finance layer on top of a pre-existing decorative two-racer animation. The merge point was a call from each horse's done action into a global random-result function. Because the old animation had no race identity, participant model, ranking, or winner, the feature shipped with three incompatible truths:

1. The UI and manual describe four teams.
2. The track creates and renders only two racers.
3. Finance chooses a separate time-seeded random team after a visual racer finishes.

The first finishing callback normally settles the bet and clears it, making the second callback a no-op by accident. Nothing is logged because all ids, figures, and graphics requests are valid; only their semantics disagree.

## Design decisions

- Keep four teams rather than reducing the UI to two. Four teams are the documented Augustus feature and have dedicated licensed UI art and descriptions.
- Choose a fair finish order once at race creation using the serialized game RNG, persist it, and make the animation realize that order. Do not use `random_from_stdlib()` for gameplay outcomes.
- Preserve the current economic deltas in the first correctness patch: a normal win adds twice the wager, a festival win adds four times the wager, and a loss subtracts the wager. Any rebalance should be a separate explicit gameplay change.
- Own the lifecycle at the hippodrome/race level, not in individual figure callbacks.
- Keep behavior and team configuration in the Augustus building/figure definitions. Graphics definitions continue to reference logical asset XML paths and must not introduce extracted PNGs into the repository.

## Implementation checklist

### 1. Characterization harness

- [ ] Add a deterministic harness around the current two routes for all four city orientations.
- [ ] Record upstream start delays, route duration, checkpoint speed changes, lap count, and finish/cooldown timing.
- [ ] Add finance characterization for normal win, festival win, loss, insufficient savings, bet locking, and donation/gift reservation.
- [ ] Add a save/load fixture with a pending bet and active legacy two-racer race.
- [ ] Confirm the current 128-unit movement normalization remains within its existing rounding tolerance; do not tune horse speed from visual impression alone.

### 2. Authoritative race model

- [ ] Introduce a named `HippodromeTeam` type with blue, red, white, and green values.
- [ ] Introduce a `HippodromeRaceState` owned by the hippodrome runtime: race serial, lifecycle phase, serialized RNG result/finish order, four participant states, winning team, and settlement flag.
- [ ] Give each participant typed team, route/lane, waypoint, lap, pace, finish tick, and figure id fields.
- [ ] Stop using `resource_id` as a binary team flag and `leading_figure_id` as a lap counter for new races.
- [ ] Move race start, winner selection, finish detection, settlement, and cooldown into one controller update. Racer figure actions should move/render their participant and report crossings only.
- [ ] Guarantee that exactly one participant records the winner and exactly one settlement occurs even if several racers cross during the same tick.

### 3. Four-team presentation

- [ ] Prototype four racers on the existing track using four stable sub-lanes or two lanes with safe sub-tile separation; verify every orientation and depth order before finalizing routes.
- [ ] Inspect the blue/red source frames to identify the livery pixels that can be palette-remapped without recoloring horses, skin, wood, or harnesses.
- [ ] Add a renderer-owned team palette/remap parameter usable by the native path now and by a Vulkan shader/push constant later.
- [ ] Define blue, red, white, and green liveries in the Augustus-owned figure/race definition while inheriting the Julius base animation assets.
- [ ] Do not generate or commit recolored copies of extracted sprites. If clean runtime remapping is impossible, pause for an explicit authored-asset decision under Augustus rather than duplicating Caesar 3 extraction output.
- [ ] Add synthetic resolution/render tests for four teams, eight directions, animation frames, cart layer, depth ordering, and four city orientations.

### 4. Betting integration

- [ ] Attach a confirmed bet to a race serial; define a pending bet as applying to the next race that has not started.
- [ ] Replace `race_result_process()` with an exactly-once settlement function accepting the authoritative winning team and race serial.
- [ ] Remove the independent four-way `random_from_stdlib()` result.
- [ ] Display the selected team, current race state, last winning team, wager, payout/loss, and result long enough to solve the usability problem reported in issue #1175.
- [ ] Keep translation keys team-based and ensure the result text names the actual winner, whether or not the player placed a bet.

### 5. Save compatibility and bridge

- [ ] Persist the race serial, phase, finish order/seed, participant state, winner, settlement flag, pending bet race serial, and last result in a versioned race packet rather than unrelated generic figure fields.
- [ ] On an old save with two active legacy racers, reconstruct a legacy race state and let that visual race finish. If a bet is pending, choose and persist its historical four-way result once during migration so repeated reloads cannot reroll it; log one bridge warning.
- [ ] On an old save with a bet but no active racers, attach it to the next new four-team race without changing savings.
- [ ] Ensure resaving writes clean new state and does not repeat the bridge warning on the next load.
- [ ] Test saves immediately before a race, during lineup, on every lap, on the finish tick, after settlement, during festival games, and after deleting/losing the hippodrome.

### 6. Statistical and runtime validation

- [ ] Run a deterministic large-sample test proving each team wins within the agreed tolerance around 25% and that lane/orientation does not bias results.
- [ ] Prove displayed first place, stored winner, notification, last-result UI, and finance settlement always name the same team.
- [ ] Prove a saved race produces the same finish order and payout after repeated reloads.
- [ ] Run repeated races with and without bets at normal speed, 1000% speed, and headlessly; fail on duplicate settlement, missing participant, unresolved graphics, warning/error, or renderer fallback.
- [ ] Run Julius-only, Julius plus Augustus, and the full Vespasian stack. Julius must not require Augustus UI assets or behavior; Augustus/Vespasian must show the complete four-team feature.
- [ ] Run the representative `.sav`/`.svv` startup gate for 3,000 ticks per save at 1000% speed, allowing only the one-time explicitly matched legacy-race bridge warning.
- [ ] Confirm steady-state reaches the existing 1000 TPS target and that four racers do not materially regress figure or draw time.

## Acceptance criteria

- All four betting choices visibly race with the correct livery.
- The visible winner is the stored winner and the only input to payout settlement.
- Odds remain unbiased at 25% per team, and payout behavior is unchanged from Augustus unless separately approved.
- Save/reload cannot change a race result or pay twice.
- Old two-racer saves remain loadable and become clean after one migrated race/save cycle.
- No generated or proprietary graphics are added to the repository.
- All mod-stack, graphics, save, soak, warning/error, fallback, and performance gates pass.
