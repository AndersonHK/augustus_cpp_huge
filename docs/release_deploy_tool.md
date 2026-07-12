# Release Deploy Tool

`tools/deploy_release_to_game.py` deploys the current local Release build into the installed Caesar 3 folder. A full deploy refreshes Mods and the runtime executable. A runtime-only deploy copies only the Release executable/debug-symbol pair for code-only testing.

The tool discovers the game folder from the Windows registry and prints the registry source and resolved game folder before it does any file work. It does not contain a hardcoded game path. Registry names and paths are handled as Unicode; localized game titles are not normalized to ASCII.

## Safety Contract

The deploy only proceeds when all of these checks pass:

- A valid Caesar 3 install folder is found from the registry.
- The game folder contains Caesar 3 markers: `c3.exe` and either `c3.eng` or `c3_mm.eng`.
- `x64/Release/Vespasian.exe` exists.

Full deploys also check:

- The target `Mods` path is either absent or resolves as a direct child of the discovered game folder.
- The target `Mods` folder is not a symlink, junction, or reparse point.
- The repo `Mods` folder contains exactly three folders: `Augustus`, `Julius`, and `Vespasian`.

After those checks pass, a full deploy moves existing target mod folders into a per-run backup folder, copies the repo mod folders in their place, restores from backup on failure when possible, copies `x64/Release/Vespasian.exe`, and copies `x64/Release/Vespasian.pdb` when that file exists. Runtime-only deploys skip all Mods validation and replacement.

## Usage

Preview the deployment without touching the game folder:

```powershell
python tools/deploy_release_to_game.py --dry-run
```

Preview a code-only runtime deployment without touching Mods:

```powershell
python tools/deploy_release_to_game.py --runtime-only --dry-run
```

Deploy the release:

```powershell
python tools/deploy_release_to_game.py
```

Deploy only the runtime executable and debug symbols:

```powershell
python tools/deploy_release_to_game.py --runtime-only
```

The script intentionally has no game-path override. If registry discovery fails, fix the installation registry entry instead of supplying a local path. It uses Python path APIs for all copy/delete operations, so spaces and non-English characters in the install path are not split or interpreted by a shell.

If a full deploy fails with `WinError 5` or access denied while moving a mod folder into the per-run backup, treat it as a Windows file-lock or permission problem. Close Caesar 3/Vespasian, Explorer windows, editors, terminals, shell previews, antivirus scans, or indexers touching the game `Mods` folder, then rerun the deploy. For code-only runtime changes, use `--runtime-only` to skip Mods entirely. Parser/XML fallback changes are not relevant to this failure.

## Manual Test Cue

`[console]::Beep()` is not reliable on this machine. When a deploy is ready for manual testing, prefer a Windows system sound:

```powershell
[System.Media.SystemSounds]::Exclamation.Play()
Start-Sleep -Milliseconds 350
[System.Media.SystemSounds]::Asterisk.Play()
```
