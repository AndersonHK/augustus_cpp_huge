# Theater employment and saved model definitions — 2026-09-06

## Finding

The composed Julius/Augustus/Vespasian theater definition requires eight employees. The worker requirement reads the shared building model, not actor visits or remaining show days. The XML layering was correct.

Older native saves wrote the entire six-integer `model_building` array (cost, four desirability values, laborers). Once building IDs became runtime-assigned, those array indices no longer identified the same buildings. The saved building-ID table cannot repair that array: the table contains compact save IDs, whereas the model writer used runtime indices.

Engineer attempt 1 26 (native version 186) contains a 12,288-byte runtime-indexed snapshot. Later native versions 202–204 wrote keyed VMO1/VMO2 records, but their writer combined explicitly marked scenario fields with every incidental difference from mod defaults. Thus those records can perpetuate earlier corruption without distinguishing intent. The affected version-203 autosave contains a theater record with all six fields marked and values `[0, 0, 0, 0, 0, 1]`.

## Implemented policy

Module definitions remain definition data. Loading constructs them from the active mod stack; saving does not snapshot them. Scenario edits are exceptions identified by building text ID and field.

Native save **205 (0xcd)** and scenario **26** use **VMO3**: entry count followed by building ID, explicit field mask, and values only for the fields in that mask. A save with no model exceptions has a 16-byte empty envelope, including the separate scenario-definition-overlay count. A theater labor exception occupies 35 bytes in total. Runtime differences are never automatically promoted to overrides. Deliberate values of zero, one, or the current default all remain valid exceptions.

Scenario actions already mark the field they modify. Manual editor changes now mark their fields too. Scenario model XML version 2 exports only marked fields and accepts omitted attributes on import. Starting a new scenario resets its overlays and reapplies mod definitions.

| Source representation | Load treatment |
| --- | --- |
| Original campaign/Julius saves predating custom model storage | Keep existing scenario/event migration; definitions come from mods. There is no full model snapshot to import. |
| Shared legacy saves with 211 fixed enum slots | Compare with the pinned source defaults and map only differences through stable text identities. |
| Early fork saves with 212 fixed enum slots | The same recovery before runtime building-ID tables (native versions through 180), including the historical `clear_trees` extension. This includes recoverable old `.svv` files; age alone is not grounds to discard an identifiable exception. |
| Native runtime-indexed snapshots | Warn and restore mod defaults: identities and edit provenance cannot be recovered reliably. |
| Native VMO1/VMO2 building-model records | Warn and discard ambiguous model records. Preserve the separate, explicitly authored scenario-definition overlays where present. |
| Native VMO3 | Apply only explicitly recorded fields over current mod definitions. |
| Dedicated post-fork Augustus schemas | Continue to require SB04 onward. This change does not claim a complete newer `.svx` semantic converter. |

Archive structure determines the source family; renaming a native save `.svx` does not evade the native repair policy. Existing saves are not overwritten by validation.

## Historical baseline and limits

`tools/generate_legacy_model_defaults.py` derives the import-only baseline from Augustus GPL source at `caa61f5ceca8cf19e1caa785e9af2a55859c52cb`. The early fork extension is pinned to `b3afad0a7`. Independent byte inspection found the complete 211-record baseline unchanged in the installed `Citizen.svx` and `Engineer.svx` (version 172); `autosave-year.svx` (174) adds the expected `clear_trees` record. `Clerk.svv` and the earliest Engineer `.svv` files also retain that recoverable 212-record layout. The generated table is a save-bridge artifact, never an alternative source of live building definitions.

Upstream version 170 did not distinguish patches that changed large-temple defaults and repaired clearing-tool defaults. The generator records earlier defaults from `075ed783b` and `991a8d6e2`. A value matching one of those historical defaults is ambiguous and inherits the active mod definition with a warning, rather than freezing an old patch value as a scenario edit. A custom edit deliberately equal to such a default cannot be distinguished without more producer provenance.

VMO1/VMO2 contain no separate provenance bit identifying whether their building-model mask was produced by an action or an incidental difference. Replaying saved events would not reconstruct the original result reliably: actions can add values, use time-dependent formulas, or run repeatedly. The separate housing, construction, migration and text overlays do have explicit provenance and are retained. No theater-specific worker clamp was introduced.

## Validation

Release game, save module, and startup parser builds pass.

The initial repaired build passed 3,000 rendered frames and canonical save/reload checks for Engineer attempt 1 26 (186), Praetor 2 9 (198), autosave-year.svv (203), Citizen.sav, and the Consul catch-up fixture. Engineer reports eight required workers and no scenario labor mask. Inspection of all five resulting version-205 saves found zero persisted model definitions, each with a 16-byte empty model envelope.

Focused contracts cover unmarked runtime differences, explicit labor values 0/1/8, sparse payload sizes, inheritance of unrelated mod fields, legacy source delta import, reset on a new scenario, repair of runtime-indexed and VMO2 theater corruption, preservation of an identifiable construction action, and sparse XML export/import. The complete catch-up suite passed on its established Consul fixture. An initial attempt to run the entire unrelated placement suite on Engineer failed its terrain-dependent proximity fixture; the model contracts themselves passed, and Engineer was then validated through the ordinary save soak.

The broad startup run exercised the 67-save required cohort. It exposed five version-172 Engineer `.svx` files with the 212-entry early-fork layout; the corrected importer then passed all five plus seven further legacy/custom cases, each with 3,000 rendered frames and strict canonical reloads. All eleven unedited legacy results contain a 16-byte empty model envelope. An isolated copy of `Citizen.svx`, edited to require three theater workers, becomes a 35-byte VMO3 envelope containing exactly `theater / MODEL_LABORERS / 3` and no other model values. The original files were not edited.

The final Consul contract suite passes, including early native fixed-enum recovery, the version-170 ambiguous-default warning, the other D08 scenario actions, placement, accounting and converted figure graphics. The final release Engineer soak passes at 1,672.9 steady-state simulation ticks/sec.

**The broad gate is not entirely green:** autosave-year.svv and autosave-year-bak-1.svv missed its 1,000 simulation ticks/sec threshold at 947.9 and 979.7 respectively, with zero warnings/errors during canonical reload and soak. The final fresh-process recheck passed the backup at 1,014.4, while the main autosave still failed at 958.1. This is the same performance boundary reported in the preceding graphics validation; it has not been waived or reported as a pass. No load/serialization failure remains in the retested cases.

The release executable and matching runtime modules were installed using the runtime-only deploy. The installed executable then passed 3,000 rendered frames each on the repaired Engineer-26 canonical save and recovered three-worker custom scenario, with completely empty stderr. Installed/release executable SHA-256: `0d2ee3d3da3053e78d4d92494fb3f79531a8a8e4939081df9a0fbb8f2c01d325`. Save DLL hash matches the release artifact. No git commit or ancestry change was made.

Logs (ignored workspace output): `out/theater-startup-gate.log/.err`, `out/theater-legacy-final.log/.err`, `out/theater-contract-final.log/.err`, `out/theater-release-soak.log/.err`, and `out/theater-installed-smoke.log/.err`.
