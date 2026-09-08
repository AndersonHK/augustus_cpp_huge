# Upstream save producer fixtures

`augustus_189_producer.patch` applies to Augustus commit `87b7d8b4f41bbb485f5c4ccf38c9961b422dd1b1`. Apply it to a separate source checkout, then build its SDL2 executable. It adds a hidden, noninteractive entry point to the real upstream loader and writer; it does not replace their serialization code. Do not apply it to the Vespasian checkout.

```
augustus.exe --produce-save "GAME_DIRECTORY" "INPUT.sav" "OUTPUT.svx" [MODE]
```

Use an original Caesar III save with clear building space and occupied houses, such as `Clerk c3.sav`. Input saves and extracted graphics are not distributed with this fixture. Output files must remain outside authored mod directories. Exit status is nonzero when loading, placement or writing fails; Windows crash dialogs and audio are suppressed.

Modes used in the September 8 audit:

| Mode | Source payload |
| --- | --- |
| omitted | Ordinary source load/write, without synthetic state |
| `delay=50`, `delay=200` | Changes only the special troops production rate |
| `fort=0` through `fort=3` | Places a mess hall and a fort using the source construction publisher and requested orientation |
| `station-return` | Model exceptions, disabled scenario actions, custom text, nonzero accounting, a station supplier carrying two sand loads, a wandering citizen and a dog |
| `station-going` | Same fixture with an unstarted supplier and no collected cargo |
| `station-combat` | Same fixture with the supplier's returning action saved as its pre-combat action; the deliberately absent opponent exercises recovery of an inconsistent combat relationship |

The native `--foreign-archive-test` checks compare imported identities, owners, cargo dispositions, model exceptions, production percentages and fort/ground coordinates with the source records. Combine it with `--load-save-test`, `--save-roundtrip-test`, `--catch-up-test`, and `--save-soak-frames 3000` to verify canonical native reload. The source fixture deliberately uses action 44, exercising the approved v189 ordering warning. This is a reproducible source-writer matrix, not certification of every historical producer or possible scenario.
