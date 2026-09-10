# Launcher and native mod settings

Status: implemented, deployed and ready for user validation. The upstream ledger remains paused pending that validation.

## Contract

- Build `Vespasian Launcher.exe` as a Windows GUI subsystem executable, with native tab/list/button controls and a generated sibling game icon.
- Maintain one selection across available/active lists whenever either contains a row; support mouse, keyboard, add/remove/reorder, missing/error colors and dependency validation. Later entries override earlier entries. Persist the complete list, including missing entries, without silently repairing the user's order. Block launching an invalid/empty stack.
- Share settings definitions, typed values, persistence, provenance and options UI between launcher and game. Bool and bounded integer settings are mod-scoped; no executable expressions. Resolve substitutions and conditional fragments before registry parsing.
- Compose XML by stable definition identity and direct child field. A supplied field replaces its entire subtree (including explicit empty nodes); absent fields inherit. Repeated sibling fields are replaced as a group. Root attributes inherit independently. Tombstones suppress a definition; restoring it starts fresh. Graphics retain their existing specialized layering contract.
- Determine whether settings are effective from surviving field provenance, not filenames or the chosen value. Disabled controls explain that later fields override the setting. Changes to effective settings apply to the running city before simulation resumes; no stale definition pointers.
- Preserve the existing Julius beggar spawn policies exactly. Augustus adds independently named, configurable ambient citizen/dog spawn groups; citizens default off. Vespasian explicitly overrides barracks construction with an empty node and always permits multiple barracks.
- Deduplicate dependent data without altering resolved content. Add `No Monuments`, containing metadata and sparse BuildingType overrides: instant construction retaining money costs, placement restrictions and the sum of all non-concrete material requirements. Architects and construction phases are removed. Concrete is not warehouse-storable, so its requirements are removed; the concrete maker has empty button/storage/production fields and zero employees. Work camps and architect guilds are hidden. Sand and brickworks remain because bricks still cost materials. Resource identities remain reserved for save compatibility.

## Work and validation

1. Shared XML/settings composition and persistence; test identity, grouped subtree replacement, explicit empty, bool/int bounds, conditional syntax, bad inputs, scoped ids and overridden-setting provenance.
2. Integrate registry loading and live-setting refresh; preserve current object ownership/save compatibility and atomic rollback on failure.
3. Native launcher/options UI, shared in-game entry, icon, root solution and deploy integration; test selection, zero rows, missing mods, dependency order and persistence.
4. Author settings/ambient walkers/No Monuments and deduplicate layer data; compare resolved output before/after deduplication.
5. Release x64 builds, parser/ABI tests, live settings regression, representative save advance/render soak with zero warnings/errors/fallbacks. Document exact results and limitations before handoff.

The existing `.svx`/`.svv` catch-up audit remains in the ledger. Any new persisted identity/settings data introduced here receives its own fork save gate and migration tests.

## Authoring syntax

Declare settings as valid XML in `mod.xml`:

```xml
<settings>
    <setting id="MB" name="Multiple barracks" type="bool" default="false" category="Difficulty"/>
    <setting id="RA" name="Retirement age" type="int(40,90)" default="50" category="Difficulty"/>
</settings>
```

Use `$RA` in an attribute and `$!MB{max_count="1"}` inside a construction tag. IDs are scoped to the declaring mod. Values are persisted in the user directory's `config/mod-settings.xml`; load order is `config/mod-list`. Both native options panels put categorized hardcoded options first, followed by mod headings in load order. The in-game Configuration window has a **Mod options** entry. Changes apply through a city snapshot, registry reload and city restoration before simulation resumes.

Dynamic figures declare a `base_type` and their own native profiles and graphics. Save version `0xc7` adds textual figure type identities alongside the previous exact profile identities, so changing mod registration order cannot silently reinterpret figures. The dog walking/portrait source sprites come from upstream Augustus commit `719f4860a6b57617fa323e84488e603e34d66911` under its asset license; the existing canine corpse presentation is reused. `tools/pack_dog_assets.py --game-root <installed game>` updates the installed walker atlas before Augustus graphics extraction; generated atlases and extracted assets stay outside the checkout.

An optional `<presentation name_key="TR_FIGURE_TYPE_DOG" portrait_path="Walkers\dog_portrait" portrait_image="dog_portrait"/>` gives a figure its own localized type label and native portrait. Undeclared presentation inherits the built-in base type; dynamically assigned IDs never index the fixed built-in portrait table. Declared portraits must resolve during startup validation.

## Integration findings

- Deduplication removed 1,500 identical inherited fields across two passes (1,337 initially and another 163 after tracking settings per field). Macro-bearing source files and literal overrides of setting-dependent fields remain explicit. Each removal preserves the composed XML; the second pass compares all combinations of referenced bool settings and integer bounds/defaults to identify variable fields.
- Selecting Julius previously reused the last selected mod's paths. Correct selection exposed two existing lower-layer dependencies: alternate statue orientations belonging to Augustus, and shared sidebar UI icons. The statue alternatives now live in Augustus. The sidebar behavior is retained, with five authored UI groups and nine unchanged PNG source assets backported into Julius. The PNGs come from `res/assets/Graphics/UI`, under `res/assets/LICENSE` (CC BY-SA 3.0); the play/pause/grid buttons are credited to Ouaz in `res/assets/Graphics/ui.xml`. These are original source assets, not runtime-extracted images or atlases.
- Temporary save validation now restores the archive resource layout before allocating fixed legacy chunks, matching the normal reader. A Julius-only stack has fewer declared resources; sizing validation from its runtime mapping incorrectly rejected otherwise valid newly written saves.
- The road-preview render fixture now temporarily funds construction and restores the treasury afterward. Bankrupt cities still undergo the same valid/invalid preview pixel assertions.
- The legacy `Engineer attempt 1 3.svx` soak exposed a native lion-tamer combat cursor advancing beyond its walking frames. Non-gladiator entertainers now persist the standing combat pose used by the original renderer. The failing city passes a fresh migration, canonical save reload, 3,000 rendered ticks and another clean roundtrip.

## Validation record

- Release x64 game, launcher, parser validator and mod-content test builds completed. The launcher uses the Windows GUI subsystem; the current launcher starts the console-subsystem game with `CREATE_NEW_CONSOLE`, retaining its logging/debugging terminal.
- Launcher self-test passes empty lists, add/remove/reorder, missing entries, dependency validation, single selection, persistence, disabled settings and hardcoded/mod option groups. Review captures are in `out/launcher-test`.
- Mod-content contracts pass against 861 composed repository definitions. All 19 No Monuments constructions retain money and summed non-concrete material costs; concrete production and storage declarations are empty. The deduplication preview finds no further identical fields to remove.
- Load/save ABI contracts and all five deployment-helper tests pass.
- Live settings were changed and restored against `Consul.svv`, with stable population and treasury, immediate retirement-age changes, and a subsequent 3,000-tick rendered soak.
- No Monuments passes startup and 3,000-tick save roundtrips for both `Consul.svv` and the developed `Praetor 2 10.svv`, with zero warnings/errors.
- The first broad save sweep covered 70 cases. Its one lion-tamer rendering failure was repaired and passed on recheck. The final startup/graphics/dependency-stack gate passes: 68 save cases, each migrated, canonically reloaded, simulated/rendered for 3,000 ticks, saved and reloaded again. All Julius, Augustus and Vespasian startup checks pass, including native figure lifecycles and the dog portrait. Logs: `out/mod-settings-final-startup.stdout.log` and `out/mod-settings-final-startup.stderr.log`.

Legacy imports can report repairs to older serialized relationships. Every tested repaired save must subsequently reload and simulate without warnings/errors; renderer compatibility fallbacks remain failures. This validates the fork's current migration path, not the divergent newer upstream SVX schemas still awaiting the ledger audit.

## User validation

Launch `D:\Games\GOG Games\Caesar 3\Vespasian Launcher.exe`. The installed game and launcher match the tested Release builds. The active stack remains Julius → Augustus → Vespasian; add No Monuments after Vespasian to exercise its overrides. Mod options are available in the launcher's second tab and in-game under Configuration → Mod options. The Augustus multiple-barracks control is disabled with Vespasian active because Vespasian supplies its own unrestricted construction field.

## Launcher/settings/dog follow-up (2026-09-05)

- Both native options panels now share 119 visible hardcoded configuration entries, including the existing User Interface and City Management options. Hardcoded settings remains a group heading; categories retain their page and section, for example City Management / Storage and Markets. Internal bookkeeping and the two mod-owned barracks/retirement options are excluded. INI keys and defaults share the game's canonical metadata. Numeric limits, increments and enum descriptions are explicit. In-game edits reuse the configuration screen's callbacks for audio, scaling, cursors and mutually exclusive weather previews without applying unrelated pending edits.
- The launcher, options panel and numeric editor scale fonts, controls and columns to the window DPI and respond to monitor DPI changes. Initial client sizes account for window decorations and available monitor space. Double-clicking an available/active row adds/removes that mod; empty-space double-clicks do nothing.
- The launcher remains a GUI executable. Start game explicitly creates the game's logging console. An isolated console probe launched through the actual Start game button confirms a visible console and correctly quoted data-root/mod arguments (`out/launcher-console-test/console-result.txt`).
- Dog movement already uses the roads/highways transient-roaming policy. Its 39-by-39 walk canvases lacked the explicit ground anchor used by native figure drawing. An authored XML bridge now applies (19, 29) to all eight directions and eight frames. It references installed extracted frames; no bitmap or generated extraction output was added to the repository.
- Launcher self-tests pass selection, double-clicks, categories, persistence, dependency/error states, and 100%, 150% and 200% DPI control bounds. A separate catalog audit maps all 82 existing INI-backed configuration checkboxes with the correct boolean types. The numeric dialog applies a value at all three scales. Captures and results: `out/launcher-dpi-test`.
- The focused Consul run passes live mod-setting changes/restoration, 3,000 rendered ticks, native save roundtrip and the existing catch-up contracts with empty stderr. The dog fixture verifies all 64 anchors and 27 road-tile transitions over its roaming lifetime. Logs: `out/launcher-dog.stdout.log` and `out/launcher-dog.stderr.log`.

- Final Release builds and deployment hashes match. Mod-content contracts still pass (863 definitions, 20 instant-monument cost contracts). The final startup gate passes all **70 save load/roundtrip/3,000-tick render-soak cases**, including recent SVV, legacy saves, original campaign SAV and dependency stacks. Expected initial legacy repairs are allowed only through canonical rewrite; the strict reload/soak reports zero unallowlisted warnings/errors and treats renderer fallbacks as failures. Logs: `out/launcher-dog-startup.stdout.log` and `out/launcher-dog-startup.stderr.log`. The active installed stack remains Julius → Augustus → Vespasian.

Changes remain uncommitted for user validation; this follow-up does not resolve another ledger decision or alter upstream ancestry.
