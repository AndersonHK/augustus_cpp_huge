# Religion effects and independent god settings

Julius owns `DISABLE_GOD_BLESSINGS` and `DISABLE_GOD_CURSES`, both initially false. Augustus and Vespasian inherit the same two settings, under the Julius header in both settings interfaces. The former combined checkbox is removed. The shared launcher/runtime preference loader imports a disabled `c3.inf` god-effects flag into both switches, but never replaces an explicit saved choice.

The switches conditionally omit effect definitions. Changing a switch in-game recompiles the definitions immediately. It prevents subsequent effects, including explicit scenario invocations of those omitted definitions; it does not undo effects that have already happened or cancel existing timers. The advisor's disabled notice appears only when no blessing or curse definitions remain.

## Julius and Augustus behavior

Compared against [Julius `59928389`](https://github.com/bvschaik/julius/blob/59928389e49845f1ccd24e6bb0a9a3b7b1648baa/src/city/gods.c) and [Augustus `87b7d8b4`](https://github.com/Keriew/augustus/blob/87b7d8b4f41bbb485f5c4ccf38c9961b422dd1b1/src/city/gods.c), including their underlying production, storage, invasion, finance, and tick functions.

| Behavior | Julius | Augustus / inherited by Vespasian |
| --- | --- | --- |
| Blessing trigger | Happiness 100, once until happiness falls below neutral | Five favor bolts, accumulated according to happiness and festivals |
| Wrath buildup | No favor buffer | Consumes favor before accumulating wrath |
| Ceres blessing / curses | Harvest with 16 blessed days; drought of 4 / 48 days | Same |
| Neptune blessing | Double export income until year-end | +50% export income for 12 months |
| Neptune minor curse | Sink all ships | Sink half and disrupt sea trade for 40 days, conditional on sea routes or active shipyard/wharf |
| Neptune major curse | Requires a sea trade route; sink ships and disrupt trade for 80 days | Also recognizes active shipyard/wharf |
| Mercury blessing | Add six loads each of wheat, vegetables, fruit, and meat to the least-stocked granary, within capacity | Refill eligible workshop inputs to three production batches and complete work already in progress |
| Mercury curses | Remove 16 loads from the fullest store / burn it | Same |
| Mars blessing | Military protection power 10 | Same |
| Mars minor curse | Campaign-defined uprising; original failure message | Adds a random 3–9-unit uprising for eligible non-original scenarios and uses the alternative failure message |
| Mars major curse | Disband the selected legion; uprising only if disbanding succeeds | Attempt uprising even without a legion |
| Venus blessing | Add 25 happiness | Sentiment boost 18, rejuvenate adults by three years, employment bonus for 35 months |
| Venus minor curse | Cap happiness at 50, reduce it by 5, health −10 | Sentiment boost −15, health −10 |
| Venus major curse | Cap happiness at 40, reduce it by 10, tiered health loss, trigger disease | Same |

Existing native fixes to formation traversal, resource identities, and storage capacity remain in the underlying operations; legacy array-index bugs are not reintroduced.

## Definitions and execution

`Mods/Julius/Gods/*.xml` supplies the original effects. `Mods/Augustus/Gods/*.xml` replaces only changed top-level fields. An empty `<blessings/>`, `<minor_curses/>`, or `<major_curses/>` clears that section. The conditional is **inside** the section so disabling an Augustus override cannot accidentally expose the inherited Julius effect.

Each God contains:

- `favor`: optional probability formula and bolt limit; an empty field disables accumulation.
- `wrath`: accumulation thresholds, amounts, cap, and favor depletion.
- `blessings`, `minor_curses`, `major_curses`: ordered effect lists.

An effect contains inclusive numeric `condition` ranges, optional `on_trigger` bookkeeping actions, and ordered `action` callbacks. Automatic processing selects the first matching effect. Explicit scenario invocations bypass automatic trigger conditions and bookkeeping. `automatic_only` and `stop_update` express the original early-campaign and unavailable-sea-trade control flow.

Actions can have their own conditions. These use a snapshot of the state at the beginning of the effect, so changing health in one action cannot cause several health bands to fire. `record_result="true"` publishes a callback's success to the `result` condition without messages overwriting it.

Example of an effect that could belong to any god:

```xml
<effect>
    <condition metric="happiness" min="100" />
    <action callback="granary_fill" loads="6">
        <resource type="wheat" />
        <resource type="vegetables" />
        <resource type="fruit" />
        <resource type="meat" />
    </action>
</effect>
```

The Religion module validates callback names, parameters, resource references, conditions, and bounds during startup. Runtime execution uses typed callback identities, numeric arguments, and bound resource IDs. No effect dispatch checks whether its owner is Mercury, Thor, Set, or another god. The existing `legacy` God attribute still reserves one of the five save-compatible god-status slots; expanding the pantheon/save-slot capacity is separate from this effect system.

## Active state and saves

Native SVV version **209** adds a 12-byte `religion_effect_payload` piece for the active trade bonus percentage and employment bonus formula. It contains only active-effect state, never God definitions or static building/module data. Existing timer fields retain their historic serialization names for compatibility but are shared effect channels: any god can activate them, with a later activation replacing the channel's prior value.

Older native saves recover the former implicit +50% export and employment formula values. Original C3/Julius saves recover the double-export flag as months remaining until year-end. Existing effects therefore survive a save/load even if mod defaults subsequently change. Invalid active magnitudes are repaired individually to the legacy channel values with warnings; the next save records the repaired state.

## Validation

Contracts cover malformed definitions, condition boundaries, parameterized callback dispatch, the four independent switch combinations in all three mod stacks, inherited setting ownership, alternative Julius/Augustus effects, old preference migration, and preservation of explicit choices. Native contracts cover rebinding effects to a different god identity, effect magnitudes, timer advancement, active-state serialization, legacy defaults, execution of all active gods' blessings/curses, and the export accounting consumer. Runtime runs and the save corpus gate are recorded in `out/religion-*.log` and `.err`.

All three mod stacks passed dedicated live-setting, callback execution, save roundtrip and 3,000-frame runs (`out/religion-{julius,augustus,vespasian}-final.log`, empty stderr). The broader build-8 corpus run completed 73 city soaks with zero canonical load/soak correctness warnings or errors. Its overall exit code was 1: Praetor 2 10 and Praetor 2 8 reached 945.9 and 960.7 simulation ticks/second respectively against the 1,000 threshold. Initial legacy-import repair warnings are recorded separately; this is not an all-gates-pass result.

The final build adds stricter rejuvenation bounds and individual active-payload repair. Definition and mod-content contracts pass in `out/religion-definitions-final.log` and `out/religion-content-final.log`. The final Julius-only roundtrip and 3,000-frame run passes with empty stderr (`out/religion-julius-build9.log`). The final repair run deliberately corrupts all three active magnitudes, requires exactly three warnings, then verifies a clean re-save/reload, restores the original city and completes 3,000 frames (`out/religion-repair-final.log`). The full 73-city gate was not repeated after these focused validation changes.

The final Release build succeeds (`out/religion-build-final.log`). The executable and its extractor/save DLLs are installed in the game folder and verified against the build by SHA-256; the installed Julius/Augustus Gods XML also matches source. The launcher with shared legacy-preference migration was rebuilt and installed earlier in this slice. Original binaries and Julius definitions remain backed up under `out/pre-religion-deployment`. No commit or ancestry merge was made.
