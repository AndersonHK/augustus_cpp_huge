# September 7 presentation fixes and continued upstream audit

The fetched target is `87b7d8b4f41bbb485f5c4ccf38c9961b422dd1b1`. There are 205 reachable upstream commits absent from local HEAD `6bcb5735a`, and the primary ledger has exactly one row for each. This record covers the fixes and audits completed in this batch. It is not final parity sign-off, and no ancestry merge has been performed.

## Reported presentation regressions

- **Wall climates:** the Julius extractor's climate-sensitive group list omitted the wall group. Extraction revision 14 emits central, northern and desert wall groups. Julius building data selects the matching group; Augustus and Vespasian inherit that choice.
- **Triumphal arch:** the original footprint contains placeholder pixels covered by a separately stored foreground pillar. The extractor now composes that pillar as a top layer in each orientation's generated image, using source metadata offsets. This knowledge stays in the extractor; the renderer requires no arch-specific draw sequence. The rendered review sheet shows both complete orientations without the exposed polygons.
- **Water-supplied ghosts:** a temporary preview building has no instantiated foundation module. Water eligibility now queries the planned building type, position and orientation, so a fountain ghost receives its covered/working state before animation selection.
- **Pavilion variation warning:** reusing a warning could start its flash while leaving its previous text intact. Repeated warnings now replace their payload immediately; flash timing remains unchanged.
- **Watchtower choices:** Augustus declares all four visual options, active/inactive states and climate variants. Generic `paired_orientation` selection preserves the style pair when rotating the camera. Construction rotation discovers these options instead of treating the watchtower as a single graphic. Odd-length option pairs fail graphics validation.

The extractor DLL also no longer links the runtime Terrain registry: terrain graphics binding is confined to game builds of the shared image loader. Extracted graphics were regenerated only in the installed game's `Mods/Julius/Graphics`; none were added to the source `Mods` tree.

## Additional audited corrections

| Upstream | Native owner and disposition |
| --- | --- |
| `a1b14e6f7` | `core/image.cpp`: clear freed records before file reads that can fail, preventing stale ownership on retry. Allocate only the main image entries. |
| `90a7a9c11` | `core/image_packer.cpp`: widen accumulated areas and multiplication inputs. A 17 × 16384² regression also exposed a null free-space-list dereference when an image exactly fills an atlas; that guard is fixed. The test allocates rectangle metadata, not pixel buffers. |
| `a2bab8c9e` | Retained Linux dependency installer no longer uses the obsolete multimedia PPA. Linux CI execution remains unverified here. |
| `a9b649d9c` | Native `Route::advanceTile` already returns when no owned route exists; no legacy zero-slot indexing remains. |
| `4eed71bff` | Fountain, well and latrines information hooks select the Health advisor. |
| `ca8470f06` | Both native elevated-figure render paths preserve the selected-building walker highlight and supplier color. |
| `0e81902b9` | Depot counts, displayed rows and button mapping agree on invalid storage, rubble and mothball filtering. |
| `719f4860a` | Multi-cell house conversion disables undo before replacing the merged owner with separate vacant lots. |
| `71138d21c`, `a0d1ab2bd` | Native combat category branches exclude passive natives and criminal-versus-criminal selection respectively. |
| `619ae51c2` | Request notifications use the authoritative available-for-request amount, suppress already dispatched requests, and post readiness before marking that dialog shown. |
| `3bdd1189d` | Earthquake advance refreshes meadow, walls and aqueducts after the underlying terrain edit. |
| `a98c2a2e5` | Current workforce calculation already performs one assignment using the mod-defined percentage. |
| `c47b0f6f5` | Formation-owned return-to-fort movement retains blocked soldiers in a retryable resting state; it does not delete them when a route is unavailable. |
| `74a82aa9b`, `42d1c66c1` | Intentionally omit dependency reversions confined to an upstream release matrix absent from this branch. This is not an exclusion of qualifying mobile/ARM hardware. |
| `5bb85d5ec` | SDL3 library-detection workaround is unnecessary for the retained activity's explicit SDL2 library list. |
| `68b6c312b` | Retained Android version generation reads `res/version.txt`, avoiding the obsolete subprocess API that upstream replaced. Device/package validation remains separate. |
| `87b7d8b4f` | Keep the native `-1` lookup contract with checked city access. Correct positive-id checks in XML/parameter rendering and return false for a missing sea route. Consume each serialized resource row even when its empire city is missing, preserving alignment for subsequent routes. |

Runtime coverage and direct user-interaction coverage are distinguished in the ledger. A successful city soak alone does not prove every depot click, advisor shortcut, earthquake redraw or undo interaction.

The additional `19bb6f58d` preview probe found a real open gap: watchman spawning still uses the legacy runtime hook, while the data-derived building preview selects a labor seeker. The graphics rotation fix does not resolve that separate preview-policy migration. Its ledger row remains open.

## Validation

- Release game, GraphicsExtractor DLL and StartupParserTest builds pass.
- Fresh Julius extraction succeeds with the new arch composition and all three wall groups.
- The rendered presentation fixture shows complete arches in both orientations, climate wall bases and four watchtower choices. It is produced under the installed game's `out/catch-up-ui`, outside the repository's authored assets.
- Focused contracts cover planned fountain water state, warning text after an actual repeat/flash, atlas accounting beyond 32-bit area, and complete old trade-row consumption despite missing empire cities. The corrected focused run completes 3,000 rendered city frames with empty stderr.
- All four watchtower choices pass four successive camera rotations, preserving their style pairs. The final isolated presentation executable completes another 3,000 rendered frames with empty stderr (`out/final-camera-runtime.log`).
- The complete StartupParserTest gate passes: 70 required/representative cohort saves plus three original-campaign/dependency-stack checks, each with 3,000 rendered frames. Canonical reloads and soaks contain zero unallowed warnings/errors and converted-renderer fallbacks. Logs: `out/final-catchup-startup-gate.log` and `.err`. Negative parser fixtures and first-load legacy repair diagnostics are intentional; they are not clean-load warnings being ignored.
- Final review consolidated the second request-ready caller into the same corrected function. The rebuilt release then passes all focused contracts and another 3,000-frame Consul soak (`out/final-release-runtime.log`, empty stderr). An installed version-102 `Citizen.sav` also passes repair, canonical save/reload and 3,000 rendered frames (`out/final-release-legacy-roundtrip.log`); its single surface-binding repair is logged on the first load and does not recur after saving.
- Runtime deployment succeeds. Installed EXE/DLL hashes and both changed building XML files match the reviewed outputs. Installed `Vespasian.exe` SHA-256: `A892F9294129685E9CF099D86ABDB5715900D67286149FAFB3CBCEC3DA7C57D6`. Generated graphics remain in the game installation.
- The final fetch still resolves to `87b7d8b4f`; Git reports `269 205`. Ledger inventory has 205 distinct matching hashes and no broken local document links. No commit or ancestry merge was made in this batch.

## Remaining sign-off blocker

D17 requires a choice for an Augustus save loaded with only Julius enabled: require the owning mod for unsupported objects, or load compatibility-only definitions that preserve existing objects without exposing new construction/spawning. The question has been presented to the user. Meaningful imported state must not be silently deleted.

SB04/SB05 semantic conversion is still unimplemented; identifying foreign layouts does not convert their object/state semantics. The remaining ledger also includes mixed-hunk audits, localization reconciliation and platform-specific validation. Those entries remain open, and the branch remains 205 commits behind. The D13 combined workforce slider and rounding/102-based efficiency changes remain explicitly deferred by the user.
