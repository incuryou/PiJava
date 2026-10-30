#!/usr/bin/env python3
"""Regression tests for source completeness and exact patch reconstruction."""
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

import package_source as pack


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="packaging-test-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name) / "baseline"
        self.root = Path(self.temp.name) / "source"
        self.out = Path(self.temp.name) / "output"
        self.base.mkdir()
        self.root.mkdir()

    def put(self, tree, name, data):
        pack.write_files(tree, {name: data})

    def build(self):
        return pack.package(self.root, self.base, self.out, "test", "a" * 64)

    def test_nested_rms_retained_root_outputs_ignored(self):
        expected = {}
        for number in range(11):
            expected[f"java/javax/microedition/rms/Record{number}.java"] = b"class Record {}\n"
            expected[f"romfs/classes/javax/microedition/rms/Record{number}.class"] = b"\xca\xfe\xba\xbe\0"
        expected["src/rms/nested.txt"] = b"nested must survive\n"
        pack.write_files(self.root, expected)
        for ignored in ("rms/runtime.db", "build/object.o", ".git/config", "tools/__pycache__/cached.pyc"):
            self.put(self.root, ignored, b"excluded")
        self.assertEqual(pack.inventory(self.root, source_policy=True), expected)
        result = self.build()
        self.assertTrue(result["ok"])
        self.assertEqual(len(result["rms_package_files"]), 22)
        with zipfile.ZipFile(self.out / "full-source.zip") as archive:
            self.assertEqual(set(archive.namelist()), set(expected) | {pack.SELF_MANIFEST})

    def test_unexplained_missing_extra_and_changed_are_failures(self):
        for actual, key, path in (({}, "missing", "a"),
                                  ({"a": b"one", "build/evil": b"bad"}, "extra", "build/evil"),
                                  ({"a": b"two"}, "changed", "a")):
            with self.subTest(key=key):
                result = pack.compare({"a": b"one"}, actual)
                self.assertFalse(result["ok"])
                self.assertEqual(result[key], [path])

    def test_linked_worktree_metadata_file_is_not_source(self):
        self.put(self.root, ".git", b"gitdir: /private/repository/worktrees/checkout\n")
        self.put(self.root, "src/game.cpp", b"int main() {}\n")
        self.assertEqual(pack.inventory(self.root, source_policy=True),
                         {"src/game.cpp": b"int main() {}\n"})
        self.assertTrue(self.build()["ok"])
        with zipfile.ZipFile(self.out / "full-source.zip") as archive:
            self.assertNotIn(".git", archive.namelist())

    def test_manifest_rejects_extra_missing_or_changed(self):
        manifest = {"files": {"a": pack.metadata(b"one")}}
        for actual in ({}, {"a": b"two"}, {"a": b"one", "extra": b"x"}):
            self.assertFalse(pack.verify_manifest(actual, manifest)["ok"])
        self.assertTrue(pack.verify_manifest({"a": b"one", pack.SELF_MANIFEST: b"self"}, manifest)["ok"])

    def test_patch_new_deleted_modified_no_final_newline_and_binary(self):
        before = {"changed.txt": b"first\nold", "removed.txt": b"no newline",
                  "removed.bin": b"\0old", "empty-deleted": b"",
                  "changed.bin": b"\0old", "line-ending.txt": b"old\r\n"}
        after = {"changed.txt": b"first\nnew", "new.txt": b"new no newline",
                 "changed.bin": b"\0new", "new.bin": b"\xff\0",
                 "empty-added": b"", "line-ending.txt": b"new\r\n"}
        pack.write_files(self.base, before)
        pack.write_files(self.root, after)
        result = self.build()
        self.assertTrue(result["ok"])
        patch = (self.out / "baseline-to-p1.patch").read_bytes()
        self.assertIn(b"\\ No newline at end of file\n", patch)
        self.assertIn(b"--- /dev/null\n+++ b/new.txt\n", patch)
        self.assertIn(b"--- a/removed.txt\n+++ /dev/null\n", patch)
        deleted = json.loads((self.out / "deleted-files.json").read_text())["files"]
        self.assertEqual(deleted, ["empty-deleted", "removed.bin", "removed.txt"])
        with zipfile.ZipFile(self.out / "binary-files.zip") as archive:
            self.assertEqual(set(archive.namelist()), {"changed.bin", "new.bin", "empty-added", "line-ending.txt"})

    def test_existing_zip_unexplained_extra_is_detected_without_filtering(self):
        self.put(self.root, "a", b"one\n")
        self.build()
        with zipfile.ZipFile(self.out / "full-source.zip", "a") as archive:
            archive.writestr("rms/should-not-be-there", b"extra")
        report = pack.verify_package(self.root, self.base, self.out)
        self.assertFalse(report["ok"])
        self.assertEqual(report["source_to_zip"]["extra"], ["rms/should-not-be-there"])
        self.assertEqual(report["baseline_patch_binary_deletions_to_zip"]["missing"], ["rms/should-not-be-there"])

    def test_all_final_newline_transitions_and_unicode_separator(self):
        before = {"both-final": b"old\n", "both-missing": b"old", "add-final": b"old",
                  "remove-final": b"old\n", "unicode": "first\u2028old".encode()}
        after = {"both-final": b"new\n", "both-missing": b"new", "add-final": b"new\n",
                 "remove-final": b"new", "unicode": "first\u2028new".encode()}
        pack.write_files(self.base, before)
        pack.write_files(self.root, after)
        self.assertTrue(self.build()["ok"])

    def test_declared_binary_overlay_tampering_is_detected(self):
        self.put(self.root, "blob.bin", b"\0original")
        self.build()
        pack.write_zip(self.out / "binary-files.zip", {"blob.bin": b"\0changed"})
        report = pack.verify_package(self.root, self.base, self.out)
        self.assertFalse(report["ok"])
        self.assertFalse(report["change_declarations"]["ok"])
        self.assertEqual(report["baseline_patch_binary_deletions_to_zip"]["changed"], ["blob.bin"])

    def test_output_under_root_rejected(self):
        with self.assertRaisesRegex(ValueError, "outside"):
            pack.package(self.root, self.base, self.root / "return", "test")


if __name__ == "__main__":
    unittest.main(verbosity=2)
