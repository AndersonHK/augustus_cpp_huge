# Augustus ![](res/julius_48.png)

[![Github Actions](https://github.com/Keriew/augustus/workflows/Build%20Augustus/badge.svg)](https://github.com/Keriew/Augustus/actions)

 **💬 Join the Augustus Community - players, mapmakers, and developers**  
[![Discord](https://img.shields.io/badge/Discord-TheZakhcolytes-5865F2?logo=discord&logoColor=white)](https://discord.gg/GamerZakh)  
kindly hosted by GamerZakh.

 **📜 Share Maps, Campaigns and Scenarios**  
[![AugustusScernarios](https://augustus.josecadete.net/badge/c3-heaven.svg)](https://caesar3.heavengames.com/downloads/lister.php?&category=augustus_scen&start=0&s=dls&o=d)  
Download Julius and Augustus scenarios, create your own and share with others! 

| Platform | Latest release | Unstable build |
|----------|----------------|----------------|
| Windows - 64 bit| Next release! |[![Download](https://augustus.josecadete.net/badge/development/windows-64bit.svg)](https://augustus.josecadete.net/download/latest/development/windows-64bit)   [(Download development assets)](https://augustus.josecadete.net/download/latest/development/assets)
| Windows - 32 bit  | [![Download](https://augustus.josecadete.net/badge/release/windows.svg)](https://augustus.josecadete.net/download/latest/release/windows)|[![Download](https://augustus.josecadete.net/badge/development/windows.svg)](https://augustus.josecadete.net/download/latest/development/windows)
| Linux AppImage | [![Download](https://augustus.josecadete.net/badge/release/linux-appimage.svg)](https://augustus.josecadete.net/download/latest/release/linux-appimage) | [![Download](https://augustus.josecadete.net/badge/development/linux-appimage.svg)](https://augustus.josecadete.net/download/latest/development/linux-appimage)
| Linux Flatpak | Next release! | [![Download](https://augustus.josecadete.net/badge/development/linux-flatpak.svg)](https://augustus.josecadete.net/download/latest/development/linux-flatpak)
| Mac | [![Download](https://augustus.josecadete.net/badge/release/mac.svg)](https://augustus.josecadete.net/download/latest/release/mac) | [![Download](https://augustus.josecadete.net/badge/development/mac.svg)](https://augustus.josecadete.net/download/latest/development/mac) |
| PS Vita | [![Download](https://augustus.josecadete.net/badge/release/vita.svg)](https://augustus.josecadete.net/download/latest/release/vita)| [![Download](https://augustus.josecadete.net/badge/development/vita.svg)](https://augustus.josecadete.net/download/latest/development/vita) |
| Switch |  [![Download](https://augustus.josecadete.net/badge/release/switch.svg)](https://augustus.josecadete.net/download/latest/release/switch) | [![Download](https://augustus.josecadete.net/badge/development/switch.svg)](https://augustus.josecadete.net/download/latest/development/switch) |
| Android APK |  [![Download](https://augustus.josecadete.net/badge/release/android.svg)](https://augustus.josecadete.net/download/latest/release/android) | [![Download](https://augustus.josecadete.net/badge/development/android.svg)](https://augustus.josecadete.net/download/latest/development/android) |


Alternatively, you can [**try Augustus in your browser**](https://augustus.josecadete.net/play/). Note that you'll still need to provide a valid Caesar 3 installation folder.


Augustus is a fork of the Julius project that intends to incorporate gameplay changes.
=======
The aim of this project is to provide enhanced, customizable gameplay to Caesar 3 using project Julius UI enhancements.

Augustus is able to load Caesar 3 and Julius saves, however saves made with Augustus **will not work** outside Augustus.

Gameplay enhancements include:
- Roadblocks
- Market special orders
- Global labour pool
- Partial warehouse storage
- Increased game limits
- Zoom controls
- And more!

Because of gameplay changes and additions, save files from Augustus are NOT compatible with Caesar 3 or Julius. Augustus is able to load Caesar 3 save files, but not the other way around. If you want vanilla experience with visual and UI improvements, or want to use save files in base Caesar 3, check [Julius](https://github.com/bvschaik/julius).

Augustus, like Julius, requires the original assets (graphics, sounds, etc) from Caesar 3 to run. Augustus optionally [supports the high-quality MP3 files](https://github.com/bvschaik/julius/wiki/MP3-Support) once provided on the Sierra website.

[![](doc/main-image.png)](https://github.com/user-attachments/assets/5027579d-4277-4b1f-9eca-297a04cb1c79)

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
