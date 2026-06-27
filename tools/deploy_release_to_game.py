#!/usr/bin/env python3
"""Deploy the current Vespasian release build into the installed Caesar 3 folder."""

from __future__ import annotations

import argparse
import traceback
import ctypes
from ctypes import wintypes
import os
import stat
import shutil
import sys
import time
from pathlib import Path


EXPECTED_MOD_FOLDERS = {"Augustus", "Julius", "Vespasian"}
GOG_GAME_IDS = {"1207658835"}
GAME_NAME_TOKENS = ("caesar 3", "caesar iii")
GAME_ROOT_REQUIRED_FILES = ("c3.exe",)
GAME_ROOT_LOCALE_FILES = ("c3.eng", "c3_mm.eng")
LEGACY_DEPLOY_WORKSPACES = ("Mods.deploy-staging", "Mods.deploy-backup")
DEPLOY_BACKUP_PREFIX = "Mods.deploy-backup"
PROCESS_SNAPSHOT_FLAG = 0x00000002
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", ctypes.c_ulong),
        ("cntUsage", ctypes.c_ulong),
        ("th32ProcessID", ctypes.c_ulong),
        ("th32DefaultHeapID", ctypes.c_void_p),
        ("th32ModuleID", ctypes.c_ulong),
        ("cntThreads", ctypes.c_ulong),
        ("th32ParentProcessID", ctypes.c_ulong),
        ("pcPriClassBase", ctypes.c_long),
        ("dwFlags", ctypes.c_ulong),
        ("szExeFile", ctypes.c_wchar * 260),
    ]


def windows_process_entries() -> list[tuple[int, int, str]]:
    if os.name != "nt":
        return []

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    snapshot = kernel32.CreateToolhelp32Snapshot(PROCESS_SNAPSHOT_FLAG, 0)
    if snapshot == ctypes.c_void_p(-1).value:
        return []

    entries: list[tuple[int, int, str]] = []
    entry = PROCESSENTRY32W()
    entry.dwSize = ctypes.sizeof(PROCESSENTRY32W)
    has_entry = kernel32.Process32FirstW(snapshot, ctypes.byref(entry))
    try:
        while has_entry:
            entries.append((entry.th32ProcessID, entry.th32ParentProcessID, entry.szExeFile))
            has_entry = kernel32.Process32NextW(snapshot, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snapshot)
    return entries


def windows_process_image_path(pid: int) -> Path | None:
    if os.name != "nt":
        return None

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    process = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not process:
        return None
    try:
        buffer = ctypes.create_unicode_buffer(32768)
        size = wintypes.DWORD(len(buffer))
        if kernel32.QueryFullProcessImageNameW(process, 0, buffer, ctypes.byref(size)):
            return Path(buffer.value).resolve()
        return None
    finally:
        kernel32.CloseHandle(process)


def game_name_matches(value: object) -> bool:
    text = str(value or "").casefold()
    return any(token in text for token in GAME_NAME_TOKENS)


def registry_value(values: dict[str, object], *names: str) -> str | None:
    lower_values = {key.casefold(): value for key, value in values.items()}
    for name in names:
        value = lower_values.get(name.casefold())
        if value:
            return str(value)
    return None


def path_from_registry_value(value: str) -> Path:
    text = os.path.expandvars(value).strip().strip('"').strip()
    return Path(text)


def registry_path_values(values: dict[str, object], *names: str) -> list[Path]:
    paths: list[Path] = []
    for name in names:
        value = registry_value(values, name)
        if value:
            paths.append(path_from_registry_value(value))
    return paths


def normalized_game_root_candidate(path: Path) -> Path:
    return path.parent if path.is_file() else path


def registry_path_points_to_game_root(values: dict[str, object]) -> bool:
    for path in registry_path_values(values, "path", "workingDir", "InstallLocation", "installLocation", "exe"):
        candidate = normalized_game_root_candidate(path)
        if candidate.is_dir() and is_valid_game_root(candidate):
            return True
    return False


def gog_entry_matches_caesar_3(game_key: str, values: dict[str, object]) -> bool:
    ids = {game_key}
    ids.update(str(values.get(name, "")) for name in ("gameID", "productID"))
    if ids & GOG_GAME_IDS:
        return True
    if any(game_name_matches(values.get(name)) for name in ("gameName", "title", "DisplayName")):
        return True
    return registry_path_points_to_game_root(values)


def uninstall_entry_matches_caesar_3(values: dict[str, object]) -> bool:
    if game_name_matches(values.get("DisplayName")):
        return True
    return registry_path_points_to_game_root(values)


def read_registry_values(winreg: object, root: int, subkey: str, view_flag: int) -> dict[str, object] | None:
    access = winreg.KEY_READ | view_flag
    try:
        with winreg.OpenKey(root, subkey, 0, access) as key:
            value_count = winreg.QueryInfoKey(key)[1]
            values: dict[str, object] = {}
            for index in range(value_count):
                name, value, _ = winreg.EnumValue(key, index)
                values[name] = value
            return values
    except OSError:
        return None


def registry_subkeys(winreg: object, root: int, subkey: str, view_flag: int) -> list[str]:
    access = winreg.KEY_READ | view_flag
    try:
        with winreg.OpenKey(root, subkey, 0, access) as key:
            subkey_count = winreg.QueryInfoKey(key)[0]
            return [winreg.EnumKey(key, index) for index in range(subkey_count)]
    except OSError:
        return []


def registry_views(winreg: object) -> tuple[int, ...]:
    if not hasattr(winreg, "KEY_WOW64_32KEY"):
        return (0,)
    return (winreg.KEY_WOW64_32KEY, winreg.KEY_WOW64_64KEY, 0)


def candidate_registry_paths() -> list[tuple[str, Path]]:
    if os.name != "nt":
        return []

    import winreg

    roots = (
        ("HKLM", winreg.HKEY_LOCAL_MACHINE),
        ("HKCU", winreg.HKEY_CURRENT_USER),
    )
    candidates: list[tuple[str, Path]] = []

    gog_roots = (
        r"SOFTWARE\GOG.com\Games",
        r"SOFTWARE\WOW6432Node\GOG.com\Games",
    )
    for root_name, root in roots:
        for view_flag in registry_views(winreg):
            for games_root in gog_roots:
                for game_key in registry_subkeys(winreg, root, games_root, view_flag):
                    subkey = rf"{games_root}\{game_key}"
                    values = read_registry_values(winreg, root, subkey, view_flag)
                    if not values:
                        continue
                    if not gog_entry_matches_caesar_3(game_key, values):
                        continue
                    path = registry_value(values, "path", "workingDir", "InstallLocation", "installLocation")
                    if path:
                        candidates.append((rf"{root_name}\{subkey}", path_from_registry_value(path)))

    uninstall_roots = (
        r"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall",
        r"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall",
    )
    for root_name, root in roots:
        for view_flag in registry_views(winreg):
            for uninstall_root in uninstall_roots:
                for app_key in registry_subkeys(winreg, root, uninstall_root, view_flag):
                    subkey = rf"{uninstall_root}\{app_key}"
                    values = read_registry_values(winreg, root, subkey, view_flag)
                    if not values or not uninstall_entry_matches_caesar_3(values):
                        continue
                    path = registry_value(values, "InstallLocation", "InstallSource")
                    if path:
                        candidates.append((rf"{root_name}\{subkey}", path_from_registry_value(path)))

    return candidates


def game_root_from_registry() -> tuple[str, Path] | None:
    for source, path in candidate_registry_paths():
        if path.is_file():
            path = path.parent
        if path.is_dir():
            root = path.resolve(strict=True)
            if is_valid_game_root(root):
                return source, root
    return None


def directory_contains_file(directory: Path, name: str) -> bool:
    target = name.casefold()
    return any(entry.is_file() and entry.name.casefold() == target for entry in directory.iterdir())


def is_valid_game_root(game_root: Path) -> bool:
    if not game_root.is_dir():
        return False
    has_required = all(directory_contains_file(game_root, name) for name in GAME_ROOT_REQUIRED_FILES)
    has_locale = any(directory_contains_file(game_root, name) for name in GAME_ROOT_LOCALE_FILES)
    return has_required and has_locale


def require_game_root(game_root: Path) -> None:
    if not is_valid_game_root(game_root):
        required = ", ".join(GAME_ROOT_REQUIRED_FILES)
        locale = " or ".join(GAME_ROOT_LOCALE_FILES)
        raise RuntimeError(f"Game folder is missing Caesar 3 markers: {required} and {locale}: {game_root}")


def is_reparse_point(path: Path) -> bool:
    attributes = getattr(path.lstat(), "st_file_attributes", 0)
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    return bool(attributes & reparse_flag)


def same_path(left: Path, right: Path) -> bool:
    try:
        return left.samefile(right)
    except OSError:
        return left.resolve() == right.resolve()


def require_mod_folder_path(mods_path: Path, label: str, expected_parent: Path | None = None) -> Path:
    if not mods_path.is_dir():
        raise RuntimeError(f"{label} Mods folder does not exist: {mods_path}")
    if mods_path.name != "Mods":
        raise RuntimeError(f"{label} Mods folder must be named exactly 'Mods': {mods_path}")
    if mods_path.is_symlink() or is_reparse_point(mods_path):
        raise RuntimeError(f"{label} Mods folder must not be a symlink, junction, or reparse point: {mods_path}")

    resolved_mods = mods_path.resolve(strict=True)
    if expected_parent and not same_path(resolved_mods.parent, expected_parent.resolve(strict=True)):
        raise RuntimeError(f"{label} Mods folder is not a direct child of the expected folder: {resolved_mods}")
    return resolved_mods


def require_exact_mod_folder(mods_path: Path, label: str, expected_parent: Path | None = None) -> Path:
    resolved_mods = require_mod_folder_path(mods_path, label, expected_parent)
    require_exact_mod_contents(resolved_mods, label)
    return resolved_mods


def require_exact_mod_contents(mods_path: Path, label: str) -> None:
    entries = list(mods_path.iterdir())
    folders = {entry.name for entry in entries if entry.is_dir()}
    non_folders = [entry.name for entry in entries if not entry.is_dir()]
    if folders != EXPECTED_MOD_FOLDERS or non_folders:
        expected = ", ".join(sorted(EXPECTED_MOD_FOLDERS))
        actual_folders = ", ".join(sorted(folders)) or "<none>"
        actual_files = ", ".join(sorted(non_folders)) or "<none>"
        raise RuntimeError(
            f"{label} Mods folder must contain exactly these folders: {expected}. "
            f"Found folders: {actual_folders}. Found files: {actual_files}."
        )


def require_safe_target_mods(target_mods: Path, game_root: Path) -> Path:
    if not target_mods.exists():
        if target_mods.name != "Mods":
            raise RuntimeError(f"Target Mods folder must be named exactly 'Mods': {target_mods}")
        if not same_path(target_mods.parent, game_root):
            raise RuntimeError(f"Target Mods folder is not a direct child of the game folder: {target_mods}")
        return target_mods
    resolved_mods = require_mod_folder_path(target_mods, "Target", game_root)
    if same_path(resolved_mods, game_root) or same_path(resolved_mods, game_root.parent):
        raise RuntimeError(f"Refusing to delete suspicious Mods path: {resolved_mods}")
    return resolved_mods


def running_processes_under(folder: Path) -> list[str]:
    root_text = str(folder.resolve()).casefold()
    matches: list[str] = []
    for pid, _, exe_name in windows_process_entries():
        process_path = windows_process_image_path(pid)
        if not process_path:
            continue
        process_text = str(process_path).casefold()
        if process_text == root_text or process_text.startswith(root_text + os.sep):
            matches.append(f"{exe_name} (pid {pid})")
    return matches


def require_no_running_game_processes(game_root: Path, dry_run: bool) -> None:
    running = running_processes_under(game_root)
    if not running:
        return
    message = "Close running Caesar 3/Vespasian processes before deploy: " + ", ".join(running)
    if dry_run:
        print(f"WARNING: {message}", file=sys.stderr)
    else:
        raise RuntimeError(message)


def copy_file(source: Path, destination: Path, dry_run: bool) -> None:
    print(f"{'Would copy' if dry_run else 'Copying'} {source} -> {destination}")
    if not dry_run:
        shutil.copy2(source, destination)


def make_writable_and_retry_remove(func: object, path: str, exc_info: object) -> None:
    try:
        os.chmod(path, stat.S_IWRITE)
        func(path)
    except OSError:
        raise


def remove_tree(path: Path, label: str) -> None:
    if not path.exists():
        return
    last_error: OSError | None = None
    for attempt in range(3):
        try:
            shutil.rmtree(path, onerror=make_writable_and_retry_remove)
            return
        except OSError as exc:
            last_error = exc
            if attempt < 2:
                time.sleep(0.25)
    raise RuntimeError(f"Unable to remove {label}: {path}. {describe_os_error(last_error)}") from last_error


def remove_path(path: Path, label: str) -> None:
    if not path.exists():
        return
    if path.is_dir() and not path.is_symlink():
        remove_tree(path, label)
        return
    try:
        path.unlink()
    except OSError as exc:
        raise RuntimeError(f"Unable to remove {label}: {path}. {describe_os_error(exc)}") from exc


def require_safe_deploy_workspace_path(path: Path, game_root: Path, label: str) -> Path:
    if not same_path(path.parent, game_root):
        raise RuntimeError(f"{label} must be a direct child of the game folder: {path}")
    allowed_name = path.name in LEGACY_DEPLOY_WORKSPACES or path.name.startswith(f"{DEPLOY_BACKUP_PREFIX}.")
    if not allowed_name:
        raise RuntimeError(f"{label} has an unexpected deploy workspace name: {path}")
    if path.exists() and (path.is_symlink() or is_reparse_point(path)):
        raise RuntimeError(f"{label} must not be a symlink, junction, or reparse point: {path}")
    return path


def remove_deploy_workspace(path: Path, game_root: Path, label: str, required: bool) -> None:
    try:
        require_safe_deploy_workspace_path(path, game_root, label)
        remove_tree(path, label)
    except RuntimeError as exc:
        if required:
            raise
        print(f"WARNING: {exc}", file=sys.stderr)


def cleanup_legacy_deploy_workspaces(game_root: Path, dry_run: bool) -> None:
    for name in LEGACY_DEPLOY_WORKSPACES:
        workspace = game_root / name
        if not workspace.exists():
            continue
        if dry_run:
            print(f"Would remove stale deploy workspace: {workspace}")
        else:
            print(f"Removing stale deploy workspace: {workspace}")
            remove_deploy_workspace(workspace, game_root, "stale deploy workspace", required=False)


def unique_deploy_backup_path(game_root: Path) -> Path:
    timestamp_ms = int(time.time() * 1000)
    for attempt in range(100):
        candidate = game_root / f"{DEPLOY_BACKUP_PREFIX}.{os.getpid()}.{timestamp_ms}.{attempt}"
        if not candidate.exists():
            return candidate
    raise RuntimeError("Unable to choose an unused deploy backup folder name.")


def require_real_directory(path: Path, label: str) -> None:
    if not path.is_dir():
        raise RuntimeError(f"{label} must be a real directory: {path}")
    if path.is_symlink() or is_reparse_point(path):
        raise RuntimeError(f"{label} must not be a symlink, junction, or reparse point: {path}")


def move_path(source: Path, destination: Path, label: str) -> None:
    try:
        source.replace(destination)
    except OSError as exc:
        raise RuntimeError(f"Unable to move {label}: {source} -> {destination}. {describe_os_error(exc)}") from exc


def copy_tree(source: Path, destination: Path, label: str) -> None:
    require_real_directory(source, f"{label} source folder")
    if destination.exists():
        raise RuntimeError(f"{label} destination already exists before copy: {destination}")
    try:
        shutil.copytree(source, destination)
    except OSError as exc:
        cleanup_detail = ""
        try:
            remove_path(destination, f"partial {label}")
        except RuntimeError as cleanup_exc:
            cleanup_detail = f" Partial copy cleanup also failed: {cleanup_exc}"
        raise RuntimeError(
            f"Unable to copy {label}: {source} -> {destination}. {describe_os_error(exc)}{cleanup_detail}"
        ) from exc


def restore_mod_folders(target_mods: Path, backup_mods: Path, original_existing: set[str]) -> list[str]:
    restore_errors: list[str] = []
    for folder in sorted(EXPECTED_MOD_FOLDERS):
        target_folder = target_mods / folder
        backup_folder = backup_mods / folder
        try:
            if backup_folder.exists():
                remove_path(target_folder, "partially deployed mod folder")
                move_path(backup_folder, target_folder, "target mod folder restore")
            elif folder not in original_existing:
                remove_path(target_folder, "new target mod folder after failed deploy")
        except RuntimeError as restore_exc:
            restore_errors.append(str(restore_exc))
    return restore_errors


def describe_os_error(exc: OSError) -> str:
    parts: list[str] = []
    if getattr(exc, "winerror", None):
        parts.append(f"WinError {exc.winerror}")
    elif getattr(exc, "errno", None):
        parts.append(f"errno {exc.errno}")
    if getattr(exc, "strerror", None):
        parts.append(str(exc.strerror))
    if getattr(exc, "filename", None):
        parts.append(f"path={exc.filename}")
    if getattr(exc, "filename2", None):
        parts.append(f"target={exc.filename2}")
    return "; ".join(parts) if parts else str(exc)


def remove_unexpected_target_mod_entries(target_mods: Path, dry_run: bool) -> None:
    for entry in list(target_mods.iterdir()):
        if entry.name in EXPECTED_MOD_FOLDERS and entry.is_dir():
            continue
        print(f"{'Would remove' if dry_run else 'Removing'} unexpected target Mods entry: {entry}")
        if not dry_run:
            remove_path(entry, "unexpected target Mods entry")


def replace_mods_folder(source_mods: Path, target_mods: Path, game_root: Path, dry_run: bool) -> None:
    backup_mods = unique_deploy_backup_path(game_root)
    if dry_run:
        print(f"Would ensure target Mods folder exists: {target_mods}")
        cleanup_legacy_deploy_workspaces(game_root, True)
        if target_mods.exists():
            remove_unexpected_target_mod_entries(target_mods, True)
        print(f"Would prepare per-run backup folder: {backup_mods}")
        for folder in sorted(EXPECTED_MOD_FOLDERS):
            print(f"Would move existing mod folder to backup if present: {target_mods / folder}")
            print(f"Would copy source mod folder directly: {source_mods / folder} -> {target_mods / folder}")
        print(f"Would remove per-run backup after successful deploy: {backup_mods}")
        return

    target_mods.mkdir(parents=False, exist_ok=True)
    cleanup_legacy_deploy_workspaces(game_root, False)
    remove_unexpected_target_mod_entries(target_mods, False)

    require_safe_deploy_workspace_path(backup_mods, game_root, "per-run deploy backup")
    print(f"Preparing per-run backup folder: {backup_mods}")
    try:
        backup_mods.mkdir()
    except OSError as exc:
        raise RuntimeError(f"Unable to create per-run deploy backup folder: {backup_mods}. {describe_os_error(exc)}") from exc
    original_existing = {folder for folder in EXPECTED_MOD_FOLDERS if (target_mods / folder).exists()}

    try:
        for folder in sorted(EXPECTED_MOD_FOLDERS):
            source_folder = source_mods / folder
            target_folder = target_mods / folder
            backup_folder = backup_mods / folder
            if target_folder.exists():
                require_real_directory(target_folder, "target mod folder")
                print(f"Moving target mod folder to backup: {target_folder} -> {backup_folder}")
                move_path(target_folder, backup_folder, "target mod folder backup")

            print(f"Copying source mod folder: {source_folder} -> {target_folder}")
            copy_tree(source_folder, target_folder, "source mod folder")

        require_exact_mod_contents(target_mods, "Target")
    except RuntimeError as exc:
        restore_errors = restore_mod_folders(target_mods, backup_mods, original_existing)
        remove_deploy_workspace(backup_mods, game_root, "backup Mods folder after failed deploy", required=False)
        if restore_errors:
            raise RuntimeError(
                "Unable to replace target mod folders, and restore from the per-run backup had errors: "
                + " | ".join(restore_errors)
            ) from exc
        raise

    if backup_mods.exists():
        remove_deploy_workspace(backup_mods, game_root, "per-run backup Mods folder after successful deploy", required=False)


def deploy_release(dry_run: bool) -> None:
    registry_result = game_root_from_registry()
    if not registry_result:
        raise RuntimeError("Unable to find a valid Caesar 3 game folder in the registry.")

    registry_source, game_root = registry_result
    require_game_root(game_root)
    print(f"Registry source: {registry_source}")
    print(f"Game folder: {game_root}")
    require_no_running_game_processes(game_root, dry_run)

    target_mods = require_safe_target_mods(game_root / "Mods", game_root)

    repo_root = Path(__file__).resolve().parents[1]
    source_mods = require_exact_mod_folder(repo_root / "Mods", "Source", repo_root)

    release_dir = repo_root / "x64" / "Release"
    exe_path = release_dir / "Vespasian.exe"
    pdb_path = release_dir / "Vespasian.pdb"
    if not exe_path.is_file():
        raise RuntimeError(f"Release executable does not exist: {exe_path}")

    print(f"Source Mods folder: {source_mods}")
    print(f"Release executable: {exe_path}")
    if pdb_path.is_file():
        print(f"Release debug symbols: {pdb_path}")
    else:
        print("Release debug symbols: not found; skipping Vespasian.pdb")

    require_no_running_game_processes(game_root, dry_run)
    replace_mods_folder(source_mods, target_mods, game_root, dry_run)

    copy_file(exe_path, game_root / exe_path.name, dry_run)
    if pdb_path.is_file():
        copy_file(pdb_path, game_root / pdb_path.name, dry_run)

    print("Deploy dry run completed." if dry_run else "Deploy completed.")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Deploy x64/Release/Vespasian.exe and the repo Mods folder into the registry-discovered Caesar 3 folder."
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the registry result, validation checks, and planned file operations without deleting or copying.",
    )
    pause_group = parser.add_mutually_exclusive_group()
    pause_group.add_argument(
        "--pause-on-exit",
        action="store_true",
        help="Prompt before closing even when not launched from Explorer.",
    )
    pause_group.add_argument(
        "--no-pause-on-exit",
        action="store_true",
        help="Never prompt before closing.",
    )
    return parser.parse_args()


def parent_process_name() -> str:
    entries = {pid: (parent_pid, exe_name) for pid, parent_pid, exe_name in windows_process_entries()}
    parent_pid = entries.get(os.getpid(), (0, ""))[0]
    return entries.get(parent_pid, (0, ""))[1].casefold() if parent_pid else ""


def should_pause_on_exit(args: argparse.Namespace) -> bool:
    if args.no_pause_on_exit:
        return False
    if args.pause_on_exit:
        return True
    return parent_process_name() == "explorer.exe"


def pause_on_exit_if_needed(args: argparse.Namespace) -> None:
    if should_pause_on_exit(args):
        try:
            input("\nPress Enter to close this window...")
        except EOFError:
            pass


def main() -> int:
    args = parse_args()
    exit_code = 0
    try:
        deploy_release(args.dry_run)
    except Exception as exc:
        log_path = Path(__file__).with_name("deploy_release_to_game.last.log")
        with log_path.open("w", encoding="utf-8") as log:
            log.write("DEPLOY FAILED\n")
            traceback.print_exception(type(exc), exc, exc.__traceback__, file=log)
        print("DEPLOY FAILED.", file=sys.stderr)
        print(f"ERROR: {exc}", file=sys.stderr)
        print(f"Failure details were written to: {log_path}", file=sys.stderr)
        exit_code = 1
    else:
        log_path = Path(__file__).with_name("deploy_release_to_game.last.log")
        with log_path.open("w", encoding="utf-8") as log:
            log.write("DEPLOY SUCCEEDED\n")
    finally:
        pause_on_exit_if_needed(args)
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
