# Empire UI restoration and reference installation — 2026-09-08

The later screenshot-matching pass supersedes the Vespasian layout decision described below: Augustus and Vespasian now share the Augustus-style XML, while Julius keeps its classic city list. See [the reference UI revision](ui_reference_revision_2026_09_08.md).

Julius again uses the fork's right-hand city cards and original bottom trade panel. Vespasian declares the same layouts with a Trade Ledger button. Augustus retains its separate accounting layout for further visual review against upstream. The classic cards include city selection, resource icons and quotas, route-opening buttons, a scrollbar, and sorting/filtering controls. The XML controls cycle through sort/filter choices; their order reversal uses a native arrow image.

The missing map cities came from a legacy asset-ID lookup that no longer supplied usable IDs. City icons now resolve named entries through each mod's `empire_map` window definition, including native animation frames in the game map. Static UI images are resolved and checked when entering the window. Startup rejects incomplete required empire window definitions, and unresolved city graphics produce logger errors instead of silently disappearing. The sidebar badge is an authored XML composition over an extracted Julius group; no extracted graphics were added to source.

The Julius route-cost path also attempted to use an undeclared resource's null string key. It now ignores undeclared slots. Augustus-only startup exposed a missing base graphics declaration for `resource_delivery`; that declaration now lives in Augustus, with Vespasian retaining its scaling override.

## Validation

Release build succeeded. The hidden `--empire-ui-test` exercises city selection, classic sort/filter controls, route confirmation, resource settings, prices, trade advisor, Trade Ledger where present, and return-to-city navigation. Final runs loaded canonical SVV 211 fixtures for Julius, Augustus, and Vespasian, rendered the UI at 1920×1080, and advanced each save for 3,000 ticks with zero warnings/errors. Screenshots were inspected. Road-drag and menu render checks passed; the separate city-water hover check skipped because these fixtures lacked suitable unobstructed visible water. The road pixel comparison temporarily disables moving weather and cloud shadows so its captures compare the road itself, restoring both settings afterward.

Local evidence: `out/empire-final-build.log`, `out/empire-{Julius,Augustus,Vespasian}-restored.log`, their stderr files, and matching BMP/PNG captures. An initial test used the wrong save path, and another used a cross-mod save; neither is counted as a successful validation. The final runs use each stack's canonical fixture. No interactive/fullscreen game was opened.

## Standalone Augustus reference

Installed official development build **4.0.0.1500-95e120d80**, using the matching Windows x64 executable package and asset package from the download service linked by upstream. The executable's embedded version and architecture, archive paths, installed file hashes, and backup hashes were checked. The final mod-stack checks ran after updating the shared SDL DLLs and extracting the updated Augustus assets in the installed game.

Preserved **4.0.0.1159-f7b6b60bc** in:

`D:\Games\GOG Games\Caesar 3\backups\augustus-4.0.0.1159-f7b6b60bc-before-2026-09-08-063618`

The backup contains the old executable/PDB, SDL DLLs, pre-update assets, configuration, and `backup-manifest.json` with SHA-256 hashes (76 files verified). The executable is the fork baseline; the assets are the current pre-update snapshot, not asserted to be the original fork-era pack. Installation metadata and download hashes are also in `out/augustus-reference-installed.json`. Existing saves, mod selection, and the old `augustus_cpp_huge.exe` were preserved. No Git commit or ancestry merge was made for this repair.
