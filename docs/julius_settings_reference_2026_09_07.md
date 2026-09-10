# Julius settings migration reference

Audited the official `bvschaik/julius` master branch on 2026-09-07 at commit `59928389e49845f1ccd24e6bb0a9a3b7b1648baa`. These are upstream defaults for a fresh configuration, not the player's saved preferences. This reference does not authorize changing unrelated settings in the current slice.

## Julius INI options

Julius defines 17 integer options and one string option. The two scale defaults are explicitly 100; the remaining integer defaults are zero through C static initialization. The configuration window supplies the scale ranges below; INI loading itself uses `atoi` without those UI bounds. Sources: [config storage and defaults](https://github.com/bvschaik/julius/blob/59928389e49845f1ccd24e6bb0a9a3b7b1648baa/src/core/config.c), [configuration window and ranges](https://github.com/bvschaik/julius/blob/59928389e49845f1ccd24e6bb0a9a3b7b1648baa/src/window/config.c).

| INI key | Julius default | UI values | Current shared Vespasian default |
| --- | --- | --- | --- |
| `gameplay_fix_immigration` | 0 | Off/on | 0 |
| `gameplay_fix_100y_ghosts` | 0 | Off/on | 0 |
| `screen_display_scale` | 100 | 50–500, step 5 | 100 |
| `screen_cursor_scale` | 100 | 100–200, step 50 | 100 |
| `ui_sidebar_info` | 0 | Off/on | 1 |
| `ui_show_intro_video` | 0 | Off/on | 0 |
| `ui_smooth_scrolling` | 0 | Off/on | 1 |
| `ui_disable_mouse_edge_scrolling` | 0 | Off/on | 0 |
| `ui_disable_map_drag` | 0 | Off/on | 0 |
| `ui_inverse_map_drag` | 0 | Off/on | 0 |
| `ui_visual_feedback_on_delete` | 0 | Off/on | 0 |
| `ui_allow_cycling_temples` | 0 | Off/on | 0 |
| `ui_show_water_structure_range` | 0 | Off/on | 1 |
| `ui_show_construction_size` | 0 | Off/on | 1 |
| `ui_highlight_legions` | 0 | Off/on | 1 |
| `ui_show_military_sidebar` | 0 | Off/on | 0 |
| `ui_show_speedrun_info` | 0 | Stored in INI; absent from configuration-window checkbox list | 0 |
| `ui_language_dir` | Empty string | Language directory selector | Empty default; current localization system also has a locale key |

The five differing shared defaults are candidates for explicit Augustus/Vespasian declarations when these controls migrate into data. They must not become Julius defaults accidentally. Existing user choices should remain higher priority than defaults.

Both immigration and 100-year-ghost fixes really are Julius options, and both are visible in its configuration window. Their current launcher/in-game mismatch is a local integration issue, not evidence that they belong to Augustus. See the earlier [legacy fix settings report](launcher_legacy_fix_settings_report_2026_09_06.md) for the separate runtime audit.

## Original game preferences are a separate source

Julius also reads the original game's binary settings. Its fresh defaults are fullscreen, 800×600, effects/speech/city sounds enabled at 100%, music enabled at 80%, game speed 90, scroll speed 70, Hard difficulty, full tooltips, warnings enabled and gods enabled. Monthly autosave and victory-video state start at zero. Player name, last advisor and personal savings are persistent state rather than mod balance definitions. Source: [original settings storage and initialization](https://github.com/bvschaik/julius/blob/59928389e49845f1ccd24e6bb0a9a3b7b1648baa/src/game/settings.c).

Do not derive these defaults from the INI catalog, and do not move save/progress fields into mod definitions. Audit each setting's actual consumer and platform behavior when it is migrated; this is a baseline inventory, not a claim that every original settings dialog has been ported.

## Workforce compatibility and current migration

Julius has no fixed-worker pool, fixed-worker percentage, global-labor toggle or retirement-age setting in its INI catalog. Working age is 20–49 inclusive. It takes 60% of the working-age population and then applies the plebeian percentage using the original integer rounding order. Sources: [working-age population](https://github.com/bvschaik/julius/blob/59928389e49845f1ccd24e6bb0a9a3b7b1648baa/src/city/population.c), [worker calculation](https://github.com/bvschaik/julius/blob/59928389e49845f1ccd24e6bb0a9a3b7b1648baa/src/city/labor.c).

The approved fixed-worker migration therefore keeps Julius age-based, gives Augustus an optional fixed-worker toggle and a 45% default, and lets Vespasian redeclare the percentage default as 38%. This approval covers the workforce portion of D13 only; mint rates and efficiency accounting remain separate decisions.

Dependency mods redeclaring a setting ID replace its declaration at the original row position and retain its original identity/group. References from either declaring namespace resolve to one effective value. Saved user values take priority over the redeclared default. Unrelated mods may still use the same short ID independently. The old INI toggle is read only when no saved mod value exists; its hidden compatibility entry keeps it intact through configuration saves until the new setting is saved. Labels no longer embed a fixed percentage; the percentage is shown by its own standard slider.

Saved values are applied after declaration replacement, using the effective bounds. Newly saved settings persist explicit choices and imported legacy preferences, not untouched defaults. Thus changing an unrelated control in Vespasian does not pin its default of 38% when loading Augustus alone. Already recorded preferences retain their precedence.

## Implementation validation

`out/worker-content.log` / `.err` passes 898 source-definition contracts, including single inherited row/identity, parent and child macro resolution, 45/38 defaults, removing the child mod, legacy toggle import and saved-value precedence. The Julius stack has neither fixed-workforce control. All 14 authored Augustus locale files had their unused old percentage-bearing label removed and still parse as JSON.

`out/worker-live.log` / `.err` passes every in-game mod-setting occurrence check, live changes/restoration, scrolled slider interaction and OK/Cancel navigation. Actual labor arithmetic is checked at both 38% and 45% with 1,000 plebeians and with a mixed plebeian/patrician population, including the existing employment blessing. Changes recalculate workforce availability and allocation immediately. The installed build completes 3,000 rendered Consul ticks with empty stderr; population and treasury remain unchanged during the setting checks. `out/worker-settings-review.jpg` confirms the single Augustus group and value 38 in Vespasian. The runner restores the player's INI byte-for-byte and the test does not persist changed mod values.

`out/worker-launcher-test/launcher-tests.txt` passes native controls, categorized settings, persistence, disabled settings, double-click add/remove, and layouts at 96/144/192 DPI. `out/worker-setting-deploy.log` records installation of the game, launcher and authored data while preserving extracted graphics. No Git commit or ancestry change was made.

`out/worker-legacy.log` / `.err` passes the legacy Clerk SAV import and 3,000 rendered ticks after a canonical SVV write/reload, followed by another save/reload. The two known serialized-binding repairs are logged during the original import; the canonical load and soak have zero warnings/errors. The original SAV is not modified.

After the persistence correction, the content contracts, launcher self-test and full live-settings/3,000-tick Consul test were rerun successfully; their log paths above contain the final results. `out/worker-final-build.log` records the final three-target build and `out/worker-final-deploy.log` records the final runtime deployment.
