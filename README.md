# Vespasian

Vespasian is a C++ fork of [Augustus](https://github.com/Keriew/augustus) and [Julius](https://github.com/bvschaik/julius), with mod-defined gameplay, graphics and UI. Original Caesar 3 game files are required and are not distributed here.

The hardware baseline is a 64-bit OS, at least 4 GB RAM, Vulkan support and at least 1 GB graphics memory. Android, iOS and Switch 2 remain potential targets on qualifying devices. PlayStation Vita, the original Switch and 32-bit builds are excluded. Platform eligibility does not establish a working port: mobile and Switch 2 toolchains, graphics integration and device validation remain outstanding.

The currently validated build is Windows x64 with Visual Studio 2022, SDL2 and Vulkan SDK headers (`VULKAN_SDK`). Its startup checks enforce the hardware baseline; the existing SDL2 rendering backend remains in use. Android/iOS project files and platform hooks are retained. Legacy Switch adapters are preserved as reference for a future Switch 2 port, with the original Switch packaging target disabled. See [platform scope and remaining work](docs/platform_scope.md).

## Running the game

Currently, you have to build it yourself. I'm mantaining a Visual Studio 2022 project for it for now. But Linux support will be added back eventually. Once it is stable enough, I'll distribute the executable on this repo, right now expect crashes.

Then you can either copy the game to the Caesar 3 folder, or run the game from an independent
folder, in which case the game will ask you to point to the Caesar 3 folder.

Note that you must have permission to write in the game data directory as the saves will be
stored there.

See [Running Julius](https://github.com/bvschaik/julius/wiki/Running-Julius) for further instructions and startup options.

## Mod Support

This fork aims to add full mod support to the Caesar 3 engine, with the goal of eventually having compatibility with Pharaoh as well. Many hardcoded values were exported to xml, and the full gameplay from Julius and Augustus are being slowly reproduced inside the improved engine made for Vespasian.

![A building-type definition xml inside a mod folder with tutorial files in the background](doc/screenshots/vespasian-mod-support.png)

## Vespasian gameplay

This fork builds on Augustus with a slower simulation rhythm, busier cities, and XML-defined gameplay and graphics. Days are twice as long, the number of days on a month are calendar accurate. Workers actually commute to their jobs, to compensate walkers have longer ranges.

Services are smart, with walkers prioritizing walking over places that haven't been visited recently. Pathfinding was added to job seekers.

The road tool now places highways with shift and blockers with ctrl. Likewise, the clear tool is now smart and comes with a repair on ctrl and tree removal mode on shift.

When placing a building, holding shift will force place it over trees and roads. Holding a road tool over water places bridges, shift for high bridges.

### Building UI and production

Vectorized fonts are now supported and rendered in full resolution. Some UIs have already been reworked, production buildings now display their storage, efficiency, and staffing from the building information panel in a standard way.

![A developed Vespasian city with the pottery workshop production panel open](doc/screenshots/vespasian-city-production.png)

### Military formations

Formations were made larger, unit ordering more natural with soldiers arranged around their legion standard. Attacking an enemy formation now causes units to chase them.

![A Vespasian legion assembled around its standard](doc/screenshots/vespasian-legion-formation.png)

## Vespasian developer notes

This fork is migrating legacy C/static-table runtime behavior toward XML-defined objects and object-owned C++ runtime relationships. Before refactoring hot paths, read:

- [Object-owned runtime refactor doctrine](docs/object_owned_runtime_refactor.md)
- [Building reference runtime architecture](docs/building_reference_runtime_architecture.md)
- [Save/load runtime bridges](docs/save_load_runtime_bridges.md)

The intended direction is to replace repeated scans, string/id lookups, static cleanup helpers, and defensive fallback layers with object-owned registration, deregistration, and lifecycle cleanup.

## Manual

Augustus changes are explained in detail in the comprehensive manual. Below you can find the links to the manual in a few language versions.

| Language | Manual |
|----------|--------|
|English   |[Download](https://github.com/Keriew/augustus/raw/master/res/manual/augustus_manual_en_4_0.pdf)|
|Chinese   |[Download](https://github.com/Keriew/augustus/raw/master/res/translated_manuals/augustus_manual_chinese_3.0.pdf)|
|French   |[Download](https://github.com/Keriew/augustus/raw/master/res/translated_manuals/augustus_manual_fr_4_0.pdf)|
|Polish   |[Download](https://github.com/Keriew/augustus/raw/master/res/translated_manuals/augustus_manual_polish_3.0.pdf)|
|Russian   |[Download](https://github.com/Keriew/augustus/raw/master/res/translated_manuals/augustus_manual_russian_3.0.pdf)|

## Bugs

See the list of [Bugs & idiosyncrasies](https://github.com/bvschaik/julius/wiki/Caesar-3-bugs) to find out more about some known bugs.
