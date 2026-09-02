# Hippodrome Racing And Betting

The hippodrome race is one simulation with four named teams: blue, red, white, and green. The figures on the track determine the finish order, and the first actual finish is the only result used to settle a bet.

## Historical Problem

The Caesar 3 race animation created two horses. Augustus later added a four-team betting window, but the animation and betting code were never joined into one model. A visual horse reaching the end called `race_result_process()`, which selected an unrelated value from 1 through 4 with the C standard-library random generator. Consequently, only two racers appeared, the visible winner could disagree with the financial result, and the second finishing callback was harmless only because the first callback cleared the bet.

The current implementation removes that independent result roll. It preserves the existing normal and festival payouts, but makes a recorded finish the authority.

## Race Lifecycle

`BuildingEntertainment::spawn_hippodrome_horses()` creates all four participants as one operation. If any figure allocation fails, it removes the partial field and does not announce or start a race. Each successful participant has a stable `bet_horse` identity carried by its existing figure resource field.

All four racers:

- line up for the same 30-tick start period;
- run the existing orientation-aware hippodrome route for six laps;
- use base speed 3 and receive a small serialized-game-RNG pace variation at route checkpoints (a speed-4 step on one quarter of updates);
- occupy four visual sub-lanes so the composed horse and cart layers do not overlap;
- report their finish once when they enter the completed state.

The source game provides two horse appearances and two cart liveries. The four teams use the four distinct horse/cart combinations through the Julius `Walkers\hippodrome_horses` bridge. This does not add copied PNGs or generated extraction output to the repository. A future palette-remap or shader-owned livery stage may give every faction a wholly independent palette without changing race identity or settlement.

## Authoritative Result And Betting

`HippodromeRace` records one participant and one finish position per team. Duplicate callbacks are idempotent. Position one becomes the race winner, and `settle_bet()` consumes that winner directly.

Betting closes as soon as a race is active. The confirmation handler rechecks this condition so a window opened just before the race cannot place a retroactive wager. A bet is settled once, then its selected team and amount are cleared. Existing economics are intentionally unchanged:

- a normal win adds twice the wager to personal savings;
- a festival win adds four times the wager;
- a loss subtracts the wager without allowing negative savings.

Settlement requires all four teams to have registered as participants. This matters for compatibility: a save made during the historical two-racer animation can finish without treating an absent white or green team as a loser. Its pending wager carries into the next complete four-team race.

## Save And Runtime Compatibility

The pending wager remains part of the existing city save data. The active field and finish order are runtime facts reconstructed from the saved horse figures. Resetting runtime state during scenario initialization or load cannot reroll a completed payout: settlement already clears the saved wager. After a mid-race load, every live horse registers its team before it can finish; if a horse was already at the finish boundary, settlement is deferred until the complete four-team field has registered.

No new save packet is required for the current behavior because no UI or finance feature consumes an unfinished race's prospective order. If persistent last-race standings are added later, the race id, finish order, and settlement status must become versioned save data rather than being inferred from generic figure fields.

## Validation Contract

The startup harness validates invalid participants, all four unique participants, actual finish order, duplicate finish idempotence, winner lookup, and clean race reset. The executable gate additionally loads Julius alone, Julius plus Augustus, and Vespasian, then round-trips and advances the required `.sav` and `.svv` set for 3,000 ticks at 1000% speed. Migration warnings are allowed only for repairs that disappear on the current-version round trip; unresolved graphics, renderer fallbacks, new warnings, and errors fail the gate.

Manual validation should still inspect all city orientations, four simultaneous sub-lanes, team readability, betting-window lockout, normal/festival wins, losses, and an old save containing an active two-racer race.

Primary references:

- `src/building/entertainment.cpp`
- `src/city/race_bet.h`
- `src/city/race_bet.cpp`
- `src/figuretype/animal.cpp`
- `src/window/race_bet.cpp`
- `Mods/Julius/Graphics/Walkers/hippodrome_horses.xml`
- `tools/startup_parser_test/main.cpp`
