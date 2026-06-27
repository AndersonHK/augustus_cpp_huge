# Release Deploy Tool

`tools/deploy_release_to_game.py` deploys the current local Release build into the installed Caesar 3 folder.

The tool discovers the game folder from the Windows registry and prints the registry source and resolved game folder before it does any file work. It does not contain a hardcoded game path. Registry names and paths are handled as Unicode; localized game titles are not normalized to ASCII.

## Safety Contract

The deploy only proceeds when all of these checks pass:

- A valid Caesar 3 install folder is found from the registry.
- The game folder contains Caesar 3 markers: `c3.exe` and either `c3.eng` or `c3_mm.eng`.
- The target game folder contains a `Mods` folder.
- The target `Mods` folder resolves as a direct child of the discovered game folder.
- The target `Mods` folder is not a symlink, junction, or reparse point.
- The target `Mods` folder contains exactly three folders: `Augustus`, `Julius`, and `Vespasian`.
- The repo `Mods` folder also contains exactly those three folders.
- `x64/Release/Vespasian.exe` exists.

After those checks pass, the script deletes the target game `Mods` folder, copies the repo `Mods` folder in its place, copies `x64/Release/Vespasian.exe`, and copies `x64/Release/Vespasian.pdb` when that file exists.

## Usage

Preview the deployment without touching the game folder:

```powershell
python tools/deploy_release_to_game.py --dry-run
```

Deploy the release:

```powershell
python tools/deploy_release_to_game.py
```

The script intentionally has no game-path override. If registry discovery fails, fix the installation registry entry instead of supplying a local path. It uses Python path APIs for all copy/delete operations, so spaces and non-English characters in the install path are not split or interpreted by a shell.

## Manual Test Cue

`[console]::Beep()` is not reliable on this machine. When a deploy is ready for manual testing, prefer a Windows system sound:

```powershell
[System.Media.SystemSounds]::Exclamation.Play()
Start-Sleep -Milliseconds 350
[System.Media.SystemSounds]::Asterisk.Play()
```
