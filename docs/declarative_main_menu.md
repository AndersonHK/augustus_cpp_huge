# Declarative main menu

The main menu is a required mod document at `UI/windows/main_menu.xml`. The configured mod stack selects the nearest complete document, in the same way as `mission_briefing.xml`. There is deliberately no compiled layout, legacy menu fallback, or synthesized default: a missing or invalid document stops startup with the declarative-window failure reason.

## Window contract

The root is a `window` with `id="main_menu"` and explicit `base_width` and `base_height`. It may also declare these hotkey actions:

- `escape_action`: action invoked by the Escape hotkey.
- `load_file_action`: action invoked by the load-file hotkey.

Both hotkey attributes are optional; omission disables that hotkey on the menu. Widget IDs are mod-owned. The engine does not require a background, a particular widget or button, particular IDs, positions, counts, labels, or graphics.

## Main-menu widget attributes

The main-menu interpreter supports image, panel, label, text-button, and image-button widgets. It uses the shared declarative geometry fields plus:

- `phase="background|foreground"`
- `coordinate_space="dialog|screen"`
- `visible_when="main_menu|not_file_dialog"` (omission means always)
- `style="outer_panel|inner_panel|solid|label|large_label"`
- `text`, `translation`, or `binding="system.version"`
- `text_alignment="left|center|right"`
- `text_offset_x`, `text_offset_y`
- `fullscreen="true"` for a cover-scaled image
- either `assetlist` plus `image`, or `image_collection` plus `image_offset`
- `width_from_text`, `width_adjust`, and `width_round_up_to` for text-measured geometry
- `max_screen_height`, `visible_if_side_margin_lt_text`, and `side_margin_text_padding` for responsive visibility
- `invert_visibility_condition="true"` to select the complement of the responsive condition

Named authored images use path-keyed graphics groups such as `UI\Main_Menu_Background`. Legacy group images remain available only when the mod explicitly names their numeric group in its own document.

Visibility conditions use `window_get_draw_id()`, the window currently being composed. When the exit popup redraws the menu beneath it, `main_menu` therefore remains true and the central card remains visible. `window_get_id()` still identifies the active window for input and navigation. The executable startup/save gate compares menu-header pixels before, during, and after the real quit popup.

## Action vocabulary

Text and image buttons declare one of these engine capabilities in `action`:

- `career.new`
- `campaign.select`
- `game.load`
- `construction_kit.open`
- `assignment_editor.open`
- `options.open`
- `application.exit`
- `hotkey.escape`

Unknown actions are rejected during declarative-window registry loading. The action vocabulary is the engine boundary; all decisions about which capabilities appear, how many times they appear, their labels, ordering, geometry, and artwork belong to the selected mod XML.

## Shipped definitions

- Julius declares the current GitHub Julius native 640x480 background behavior, button geometry/actions, and responsive version treatment without an Augustus decorator panel.
- Augustus declares the marble-palace background, Augustus banner, current six-button layout, and version panel.
- Vespasian declares its authored 3840x2160 Roman-city background, transparent gold Vespasian banner, current six-button layout, and version panel.
