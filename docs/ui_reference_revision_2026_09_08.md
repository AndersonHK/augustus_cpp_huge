# UI reference revision — 2026-09-08

The four supplied Augustus screenshots guide the empire, finance, religion and god-powers windows. Augustus uses its original UI fonts. Vespasian inherits the same window definitions and keeps its vector fonts; identical Vespasian empire overrides were removed. Julius retains its restored classic city list and its separate advisor layouts.

## Presentation and reusable primitives

- Empire: masked, seamless stone fill; framed city-name badges linking to each city’s ledger; resource icons and quota badges; native navigation icons; sorting, filtering and historical-year dropdowns; Trade History. Live filters and sorting use current city state; historical views use archived route state and reject live resource/route edits.
- Finance: title icon, sunken white-text tax panel, native tax arrows, compact accounting columns, year selectors and rules above totals.
- Religion: native title icon, grouped table headings, sunken gods table, festival illustration and properly sized god-powers button.
- Powers of the Gods: six portrait tabs, selected portrait frames, inset descriptions and the original gold scrollbar appearance. Portrait references are optional Religion presentation data. Additional religions retain pagination.
- Shared XML support now includes tiled image panels, inset/solid panels, dropdown choice lists with bounded height and mouse-wheel scrolling, hover images, selection-border padding, proportional horizontal positioning, and data-specified scrollbar caps/track/grip. Advisor screen borders are an optional XML frame.

The new Pantheon portrait is an authored XML composition of existing extracted image groups. No extracted bitmap, new font dependency, or generated graphics tree was added to source.

## Validation and deployment

Release build succeeded. Actual 3840×2160 captures were inspected for Augustus and Vespasian, including all four requested windows and an expanded finance year selector. Captures use existing save fixtures, so city names, routes, balances and god opinions differ from the supplied screenshots.

`--empire-ui-test` checks city selection, sorting/filtering, route confirmation, resource controls, prices/advisor/return navigation, historical-year selection, historical edit prevention, Trade History and city-badge ledger navigation. It also checks tax changes/restoration, year-menu input, a 100-choice dropdown in a compact viewport, all god portrait tabs, and scrollbar wheel/drag behavior.

Final UI runs for Julius, Augustus and Vespasian each loaded a canonical SVV and advanced/rendered 3,000 ticks with zero warnings/errors. The XML/definition contract suite passed, including inherited-definition deduplication. The two original SAV fixtures (Citizen and Clerk c3) require existing surface-binding import repairs, logged as one and two warnings respectively; their canonical writes, reloads and subsequent sequential 3,000-tick soaks passed without further warnings or errors. Initial raw-import runs were not counted as clean passes.

One earlier road pixel-comparison check failed intermittently after the UI checks had passed; its unchanged rerun and all final three-stack checks passed. That isolated failure remains visible in `out/reference-Augustus-verified.err`; no comparison threshold was relaxed. City-water-hover checks skip when a fixture has no suitable visible water.

Evidence: `out/reference-ui-build12.log`, `out/reference-startup-definitions2.log`, `out/reference-gate-{Julius,Augustus,Vespasian}.log`, `out/reference-legacy-roundtrip.log`, and corresponding stderr files. Full-resolution BMPs and JPEG previews are under `out/reference-{Augustus,Vespasian,Julius}-verified/`.

The tested executable/PDB and authored data are deployed to `D:/Games/GOG Games/Caesar 3`. Testing used hidden windowed processes and an isolated configuration. The separate official Augustus installation and fork-baseline backup remain intact. No commit or ancestry merge was made.
