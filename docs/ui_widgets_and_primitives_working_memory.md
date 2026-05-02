# UI Widgets And Primitives Working Memory

Snapshot: 2026-04-25

## Purpose

This note captures the current UI widget/primitives vocabulary and the findings from the ad-hoc UI deduplication pass. Read it before planning UI cleanup, C-to-C++ UI migrations, or new shared control work.

## Vocabulary

- Primitives are draw-level building blocks. They submit low-level sprite, border, panel, text, strip, slider, or saved-region drawing through the request-based renderer path.
- Widgets are control-level composition objects. They own reusable UI behavior and layout for a recognizable control such as a bordered button, image button, advisor text button, panel, label, scrollbar dot, or top-menu panel.
- `SharedUiRuntime` is the C++ facade/orchestrator. It should expose narrow entry points to widgets for legacy callers, while keeping widget behavior in widget classes.
- Legacy C APIs such as `button_border_draw`, `outer_panel_draw`, `image_buttons_draw`, and `scrollbar_draw` are compatibility paths. They are acceptable as adapters, but new reusable composition should move toward widget/runtime methods.

## Current Widget And Primitive Stock

Core widget classes currently include:

- `UiWidget`
- `ButtonWidget`
- `BorderedButtonWidget`
- `ImageButtonWidget`
- `PanelWidget`
- `LabelWidget`
- `TopMenuPanelWidget`
- `ScrollbarWidget`
- `AdvisorTextButtonWidget`
- `AdvisorCardButtonWidget`
- `EmpireTradeRouteButtonWidget`

Current draw-level primitives include:

- `UiPrimitives`
- `UiBorderPrimitive`
- `UiOneRowBorderPrimitive`
- `UiPanelPrimitive`
- `UiSpritePrimitive`
- `UiTextPrimitive`
- `UiTiledStripPrimitive`
- `UiSliderPrimitive`
- `UiSavedRegionPrimitive`

Important distinction:

- Do not introduce a new widget before checking whether one of the existing widget classes can be extended cleanly.
- Do not move control behavior into primitives. Primitives should remain draw methods/building blocks.

## Ad-Hoc UI Tally Outside `src/graphics`

The deduplication survey counted common non-graphics call sites roughly as:

- `244` `button_border_draw` calls
- `176` `outer_panel_draw` calls
- `115` `inner_panel_draw` calls
- `75` label calls
- `6` one-row border calls
- isolated slider/unbordered panel calls

Panels, labels, image buttons, scrollbars, and sliders already route through stock widget/runtime adapters. The most useful low-risk cleanup target is repeated "bordered button plus simple content" composition in `src/window` and `src/widget`.

## 2026-04-25 Deduplication Pass

The first safe migration pass deliberately avoided adding a new icon-button class. Instead:

- `BorderedButtonWidget` gained an optional `BorderedButtonIconSpec` for single-icon content with explicit offset and optional logical size/color.
- `SharedUiRuntime` gained C++ methods for bordered icon buttons and for the existing advisor/empire button widgets.
- `src/window/advisor/military.c` and `src/window/advisor/imperial.c` were converted to `.cpp` using the established `figure.cpp` style: C++ includes first, legacy headers inside `extern "C"`.
- Military legion action icon buttons now use the extended bordered button widget through `SharedUiRuntime`.
- Trade advisor policy icon buttons now use the extended bordered button widget through `SharedUiRuntime`.
- Trade advisor footer buttons now use `AdvisorTextButtonWidget` through `SharedUiRuntime`.
- Imperial advisor donate/gift footer buttons now use `AdvisorTextButtonWidget` through `SharedUiRuntime`.

The pass intentionally left custom rows with complex conditional text/data layout alone unless they matched an existing widget with no special cases.

## 2026-05-02 Storage Window Cleanup

- `src/window/building/distribution.cpp` and `src/window/building/depot.cpp` now follow the C++ conversion path used by other migrated windows.
- Distribution permission buttons use a small construction helper instead of C-only designated compound literals.
- Depot resource-row buttons are initialized from a compact row/column loop instead of a 24-entry repeated literal table.

## 2026-05-02 Declarative Mission Briefing Window

- Declarative UI window definitions now start at `Mods/<selected mod>/UI/windows/mission_briefing.xml`.
- `src/graphics/declarative_window.h/.cpp` loads required window XML at startup and constructs `DeclarativeWindow` objects from widget declarations.
- The first migrated sample is `src/window/mission_briefing.cpp`: XML owns static panel, label, objective slot, rich text, scrollbar, and button geometry; C++ still owns scenario data, bindings, callbacks, and audio/video behavior.
- Declarative mission text uses integer `font_size_delta` values. Do not use fractional text scale for these windows; vector text should open the nearest integer font size and keep `line_spacing` explicit.
- Mission rich text can use `paragraph_spacing` for extra blank lines after paragraph and line-break tags without loosening every wrapped line.
- Stretch-height widgets can use `stretch_to_widget` with optional `stretch_margin_y` to stop before another widget, such as bottom-aligned buttons.
- The selected mod's mission briefing XML is required. Missing or invalid required widgets fail startup through the retained init-failure path.
- Named-asset image buttons draw the exact named asset and no longer apply legacy sprite-strip state offsets unless the caller uses numeric image collections. This keeps single-image UI assets such as pause/play stable when pressed.

## Migration Guidance For Future UI Tasks

- Start by inspecting existing widget classes and primitives before proposing new classes.
- Prefer small widget extensions when the visual/control contract is already represented by an existing widget.
- Add a new widget only when the target control has a distinct reusable behavior/layout contract, not merely because one caller has unusual coordinates.
- Keep `ui_runtime.cpp` as orchestration/facade glue; avoid turning it into a logic monolith.
- Use `ui_runtime_api.h` only when C-callable compatibility is needed. C++ window files should usually call `SharedUiRuntime` directly.
- When converting C UI files to C++, follow the `figure.cpp` include/linkage style and add explicit enum/integer casts where needed.
- Keep CRLF line endings on touched files.
- Do not build unless the user explicitly asks in the current chat.

## Safe Next Candidates

Good future cleanup candidates:

- More advisor footer/text buttons that already hand-compose `button_border_draw` plus centered text.
- Small icon buttons where border plus one image can use `BorderedButtonIconSpec`.
- Existing direct construction of `AdvisorTextButtonWidget` / `AdvisorCardButtonWidget` in advisor windows can be routed through `SharedUiRuntime`.

Avoid as first-pass targets:

- Religion advisor/building/figure work when another session is active there.
- Custom request/resource rows with multiple conditional text segments and resource calculations.
- Rewriting panels/labels/scrollbars that already travel through stock adapters unless there is a concrete rendering bug.
