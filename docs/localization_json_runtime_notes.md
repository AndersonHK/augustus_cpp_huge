# Localization JSON Runtime Notes

## 2026-05-04 Julius extraction to JSON project keys

- Julius legacy language files are still the source of base-game localization, but the extractor now writes them into `Mods/Julius/Localization/<locale>.json` using the same top-level JSON shape as Augustus: empty `main_strings` and `editor_strings`, structured `messages`, and flat `project_keys`.
- Legacy group/index string slots are emitted as flat project keys. When a slot already has a matching `TR_*` enum, the generated key uses that enum name. Otherwise it uses an explicit fallback key in the form `main_strings.<group>.<index>` or `editor_strings.<group>.<index>`.
- If two Julius legacy slots map to the same `TR_*` key with different extracted text, the first keeps the enum key and the later conflict is emitted with its explicit `main_strings.*` or `editor_strings.*` key. That preserves the data without inventing a new enum.
- During locale load, mods are merged in mod-list order. Later mods replace the same `TR_*`, `main_strings.*`, or `editor_strings.*` key, so Augustus JSON can override Julius values only where the key actually conflicts.
- Project keys that correspond to old legacy slots also populate the runtime legacy string maps. This replaces the hard-coded small temple, editor, and Augustus-added building localization cases that used to live in `src/core/lang.cpp`.
- The extraction writes to the Julius localization folder at runtime from the installed Caesar language files. This workspace does not include the `c3*.eng`/`c3*.rus` source binaries, so no concrete generated Julius locale JSON was created in this pass.
