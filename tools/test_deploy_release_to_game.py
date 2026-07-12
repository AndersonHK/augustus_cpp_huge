from __future__ import annotations

import hashlib
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import deploy_release_to_game as deploy


def write_file(path: Path, contents: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents, encoding="utf-8")


def tree_manifest(root: Path) -> dict[str, str]:
    manifest: dict[str, str] = {}
    for path in root.rglob("*"):
        if path.is_file():
            manifest[str(path.relative_to(root))] = hashlib.sha256(path.read_bytes()).hexdigest()
    return manifest


class DeployReleaseTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.repo = self.root / "repo"
        self.game = self.root / "game"
        self.source_mods = self.repo / "Mods"
        self.target_mods = self.game / "Mods"
        self.game.mkdir()
        self.source_mods.mkdir(parents=True)
        self.target_mods.mkdir()

        for mod in sorted(deploy.EXPECTED_MOD_FOLDERS):
            write_file(self.source_mods / mod / "definition.xml", f"new {mod}")
            write_file(self.target_mods / mod / "definition.xml", f"old {mod}")
            write_file(self.target_mods / mod / "stale.xml", f"stale {mod}")

        for index in range(150):
            write_file(
                self.target_mods / "Augustus" / "Graphics" / "Generated" / f"image_{index:04}.xml",
                f"generated {index}",
            )
        write_file(
            self.target_mods / "Augustus" / "Graphics.graphics_extract.stamp",
            "extracted",
        )
        write_file(
            self.source_mods / "Julius" / "Graphics" / "Mod_Authored" / "Bridge.xml",
            "new bridge",
        )
        write_file(
            self.target_mods / "Julius" / "Graphics" / "Mod_Authored" / "Bridge.xml",
            "old bridge",
        )
        write_file(
            self.target_mods / "Julius" / "Graphics" / "Generated" / "legacy.xml",
            "generated legacy",
        )

    def tearDown(self) -> None:
        self.temp.cleanup()

    def test_overlay_preserves_extracted_graphics_and_replaces_managed_content(self) -> None:
        generated_before = tree_manifest(self.target_mods / "Augustus" / "Graphics")

        deploy.replace_mods_folder(self.source_mods, self.target_mods, self.game, False)

        for mod in sorted(deploy.EXPECTED_MOD_FOLDERS):
            self.assertEqual((self.target_mods / mod / "definition.xml").read_text(encoding="utf-8"), f"new {mod}")
            self.assertFalse((self.target_mods / mod / "stale.xml").exists())
        self.assertEqual(tree_manifest(self.target_mods / "Augustus" / "Graphics"), generated_before)
        self.assertEqual(
            (self.target_mods / "Augustus" / "Graphics.graphics_extract.stamp").read_text(encoding="utf-8"),
            "extracted",
        )
        self.assertEqual(
            (self.target_mods / "Julius" / "Graphics" / "Mod_Authored" / "Bridge.xml").read_text(encoding="utf-8"),
            "new bridge",
        )
        self.assertEqual(
            (self.target_mods / "Julius" / "Graphics" / "Generated" / "legacy.xml").read_text(encoding="utf-8"),
            "generated legacy",
        )
        self.assertFalse(any(self.game.glob(f"{deploy.DEPLOY_BACKUP_PREFIX}.*")))

    def test_failed_overlay_restores_the_exact_target_tree(self) -> None:
        before = tree_manifest(self.target_mods)
        real_overlay = deploy.overlay_source_graphics

        def fail_for_julius(source: Path, target: Path) -> None:
            if source.parent.name == "Julius":
                raise RuntimeError("simulated copy failure")
            real_overlay(source, target)

        with mock.patch.object(deploy, "overlay_source_graphics", side_effect=fail_for_julius):
            with self.assertRaisesRegex(RuntimeError, "simulated copy failure"):
                deploy.replace_mods_folder(self.source_mods, self.target_mods, self.game, False)

        self.assertEqual(tree_manifest(self.target_mods), before)
        self.assertFalse(any(self.game.glob(f"{deploy.DEPLOY_BACKUP_PREFIX}.*")))

    def test_target_mods_must_be_directly_under_the_game_root(self) -> None:
        outside = self.root / "outside" / "Mods"
        outside.mkdir(parents=True)
        with self.assertRaisesRegex(RuntimeError, "direct child"):
            deploy.require_safe_target_mods(outside, self.game)

    def test_unexpected_target_mod_is_refused_and_not_deleted(self) -> None:
        custom_file = self.target_mods / "MyCustomMod" / "keep.txt"
        write_file(custom_file, "do not delete")

        with self.assertRaisesRegex(RuntimeError, "does not own"):
            deploy.replace_mods_folder(self.source_mods, self.target_mods, self.game, False)

        self.assertEqual(custom_file.read_text(encoding="utf-8"), "do not delete")

    def test_interrupted_backup_is_refused_and_not_deleted(self) -> None:
        interrupted = self.game / f"{deploy.DEPLOY_BACKUP_PREFIX}.previous"
        write_file(interrupted / "evidence.txt", "keep for recovery")

        with self.assertRaisesRegex(RuntimeError, "interrupted deploy backup"):
            deploy.replace_mods_folder(self.source_mods, self.target_mods, self.game, False)

        self.assertEqual((interrupted / "evidence.txt").read_text(encoding="utf-8"), "keep for recovery")


if __name__ == "__main__":
    unittest.main()
