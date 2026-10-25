#!/usr/bin/env python3
"""Package source, declare every baseline change, and verify exact reconstruction."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

SELF_MANIFEST = "SOURCE-MANIFEST.json"
ROOT_OUTPUT_DIRS = frozenset({"build", "rms"})
CACHE_DIRS = frozenset({".git", "__pycache__"})
POLICY = {
    "root_output_directories": sorted(ROOT_OUTPUT_DIRS),
    "cache_directories_at_any_depth": sorted(CACHE_DIRS),
    "self_manifest": SELF_MANIFEST,
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def metadata(data):
    return {"bytes": len(data), "sha256": sha(data)}


def inventory(root, source_policy=False):
    """Only source input gets exclusions; extracted output comparisons get none."""
    root = Path(root)
    files = {}
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        parts = relative.parts
        if source_policy and (
            parts[0] in ROOT_OUTPUT_DIRS
            or any(part in CACHE_DIRS for part in parts[:-1])
            or relative.as_posix() == SELF_MANIFEST
        ):
            continue
        if path.is_symlink():
            raise ValueError("Symlinks are not supported: " + relative.as_posix())
        if path.is_file():
            files[relative.as_posix()] = path.read_bytes()
    return files


def compare(expected, actual):
    """Bidirectional file-set and byte comparison, with no inventory filtering."""
    missing = sorted(expected.keys() - actual.keys())
    extra = sorted(actual.keys() - expected.keys())
    changed = sorted(name for name in expected.keys() & actual.keys()
                     if expected[name] != actual[name])
    return {"ok": not (missing or extra or changed), "expected_files": len(expected),
            "actual_files": len(actual), "missing": missing, "extra": extra,
            "changed": changed}


def verify_manifest(files, manifest):
    expected = manifest["files"]
    actual = {name: metadata(data) for name, data in files.items()
              if name != SELF_MANIFEST}
    return compare(expected, actual)


def is_text(data):
    # CR bytes use the binary overlay, avoiding platform-dependent line endings.
    if any(byte < 32 and byte not in (9, 10) or byte == 127 for byte in data):
        return False
    try:
        data.decode("utf-8")
    except UnicodeDecodeError:
        return False
    return True


def text_patch(name, before, after, existed_before=True, exists_after=True):
    """Produce standard unified diffs, including missing-final-newline markers."""
    def lf_lines(data):
        parts = data.decode("utf-8").split("\n")
        return [part + "\n" for part in parts[:-1]] + ([parts[-1]] if parts[-1] else [])
    old = lf_lines(before)
    new = lf_lines(after)
    output = []
    for line in difflib.unified_diff(
        old, new,
        fromfile="a/" + name if existed_before else "/dev/null",
        tofile="b/" + name if exists_after else "/dev/null",
        lineterm="\n",
    ):
        if line.endswith("\n"):
            output.append(line)
        else:
            output.append(line + "\n\\ No newline at end of file\n")
    return "".join(output).encode("utf-8")


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def write_files(root, files):
    root = Path(root)
    for name, data in files.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def write_zip(path, files):
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)


def extract_zip(path, target):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(set(names)) != len(names):
            raise ValueError("Duplicate ZIP member")
        for name in names:
            parts = Path(name).parts
            if not parts or Path(name).is_absolute() or ".." in parts:
                raise ValueError("Unsafe ZIP member: " + name)
        archive.extractall(target)


def declarations(baseline, final):
    patch = bytearray()
    binary_files = {}
    changes = []
    binary_changes = []
    deleted = []
    for name in sorted(baseline.keys() | final.keys()):
        before, after = baseline.get(name), final.get(name)
        if before == after:
            continue
        operation = "added" if before is None else "deleted" if after is None else "modified"
        binary = not all(is_text(data) for data in (before, after) if data is not None)
        entry = {"path": name, "operation": operation,
                 "encoding": "binary" if binary else "utf-8",
                 "before": metadata(before) if before is not None else None,
                 "after": metadata(after) if after is not None else None}
        changes.append(entry)
        if operation == "deleted":
            deleted.append(name)
        if binary:
            binary_changes.append(entry)
            if after is not None:
                binary_files[name] = after
        else:
            patch.extend(text_patch(name, before or b"", after or b"",
                                    before is not None, after is not None))
            # Unified diff cannot represent an added/deleted empty file.
            if not before and not after and after is not None:
                binary_files[name] = after
                entry["encoding"] = "binary"
                binary_changes.append(entry)
    return bytes(patch), binary_files, changes, binary_changes, deleted


def reconstruct(baseline, out, target):
    write_files(target, baseline)
    patch = out / "baseline-to-p1.patch"
    log = "No text changes.\n"
    if patch.stat().st_size:
        executable = shutil.which("patch")
        if not executable:
            raise RuntimeError("GNU-compatible patch is required for independent reconstruction")
        result = subprocess.run([executable, "--batch", "--fuzz=0", "-p1", "-i", str(patch.resolve())],
                                cwd=target, capture_output=True, text=True)
        log = result.stdout + result.stderr
        if result.returncode:
            raise RuntimeError("Patch reconstruction failed:\n" + log)
    extract_zip(out / "binary-files.zip", target)
    for name in json.loads((out / "deleted-files.json").read_text())["files"]:
        if not Path(name).parts or Path(name).is_absolute() or ".." in Path(name).parts:
            raise ValueError("Unsafe declared deletion: " + name)
        path = Path(target) / name
        if path.exists():
            path.unlink()
    return log


def package(root, baseline_root, out, version, nro_sha256=None):
    root, baseline_root, out = Path(root).resolve(), Path(baseline_root).resolve(), Path(out).resolve()
    if out == root or root in out.parents or out == baseline_root or baseline_root in out.parents:
        raise ValueError("Output directory must be outside source and baseline trees")
    if root == baseline_root:
        raise ValueError("Source and baseline must be distinct")
    source = inventory(root, source_policy=True)
    baseline = inventory(baseline_root, source_policy=True)
    if (baseline_root / SELF_MANIFEST).is_file():
        baseline[SELF_MANIFEST] = (baseline_root / SELF_MANIFEST).read_bytes()
    manifest = {"version": version, "nro_sha256": nro_sha256,
                "inventory_policy": POLICY,
                "files": {name: metadata(data) for name, data in source.items()}}
    manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")
    final = dict(source, **{SELF_MANIFEST: manifest_bytes})
    out.mkdir(parents=True, exist_ok=True)
    (root / SELF_MANIFEST).write_bytes(manifest_bytes)
    write_zip(out / "full-source.zip", final)
    patch, binary_files, changes, binary_changes, deleted = declarations(baseline, final)
    (out / "baseline-to-p1.patch").write_bytes(patch)
    write_zip(out / "binary-files.zip", binary_files)
    write_json(out / "source-changes.json", {"changes": changes})
    write_json(out / "binary-changes.json", {"changes": binary_changes})
    write_json(out / "deleted-files.json", {"files": deleted})
    write_json(out / "baseline-manifest.json", {"files": {name: metadata(data) for name, data in baseline.items()}})
    report = verify_package(root, baseline_root, out)
    if not report["ok"]:
        raise RuntimeError("Source equivalence failed; see source-tree-equivalence.json")
    return report


def verify_package(root, baseline_root, out):
    """Check an existing bundle without regenerating or trusting its file lists."""
    root, baseline_root, out = Path(root), Path(baseline_root), Path(out)
    source = inventory(root, source_policy=True)
    baseline = inventory(baseline_root, source_policy=True)
    if (baseline_root / SELF_MANIFEST).is_file():
        baseline[SELF_MANIFEST] = (baseline_root / SELF_MANIFEST).read_bytes()
    manifest_bytes = (root / SELF_MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    final = dict(source, **{SELF_MANIFEST: manifest_bytes})
    expected_patch, expected_binary_files, changes, binary_changes, deleted = declarations(baseline, final)
    with tempfile.TemporaryDirectory(prefix="source-package-check-") as temp:
        unpacked, rebuilt = Path(temp) / "unpacked", Path(temp) / "rebuilt"
        unpacked.mkdir()
        rebuilt.mkdir()
        extract_zip(out / "full-source.zip", unpacked)
        zip_files = inventory(unpacked)
        reconstruction_log = reconstruct(baseline, out, rebuilt)
        rebuilt_files = inventory(rebuilt)
        with zipfile.ZipFile(out / "binary-files.zip") as archive:
            actual_binary_files = {name: archive.read(name) for name in archive.namelist()}
        declarations_match = {
            "ok": (
                json.loads((out / "source-changes.json").read_text()) == {"changes": changes}
                and json.loads((out / "binary-changes.json").read_text()) == {"changes": binary_changes}
                and json.loads((out / "deleted-files.json").read_text()) == {"files": deleted}
                and json.loads((out / "baseline-manifest.json").read_text()) == {
                    "files": {name: metadata(data) for name, data in baseline.items()}}
                and (out / "baseline-to-p1.patch").read_bytes() == expected_patch
                and compare(expected_binary_files, actual_binary_files)["ok"]),
            "method": "recompute every addition/modification/deletion from baseline and source bytes",
        }
        report = {
            "inventory_policy": POLICY,
            "source_to_zip": compare(final, zip_files),
            "source_to_manifest": verify_manifest(source, manifest),
            "zip_to_manifest": verify_manifest(zip_files, manifest),
            "baseline_patch_binary_deletions_to_zip": compare(zip_files, rebuilt_files),
            "change_declarations": declarations_match,
            "baseline_file_count": len(baseline),
            "declared_changes": len(changes),
            "declared_binary_changes": len(binary_changes),
            "declared_deletions": deleted,
            "rms_package_files": sorted(name for name in source if name.startswith(
                ("java/javax/microedition/rms/", "romfs/classes/javax/microedition/rms/"))),
        }
        report["ok"] = all(report[key]["ok"] for key in (
            "source_to_zip", "source_to_manifest", "zip_to_manifest", "baseline_patch_binary_deletions_to_zip",
            "change_declarations"))
        write_json(out / "source-tree-equivalence.json", report)
        (out / "patch-reconstruction.log").write_text(reconstruction_log, encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--version", default="0.2.43-p1-r01")
    parser.add_argument("--verify", action="store_true", help="verify an existing bundle without repackaging")
    parser.add_argument("--nro-sha256", help="SHA256 of the unchanged release NRO")
    args = parser.parse_args()
    if args.nro_sha256 and (len(args.nro_sha256) != 64 or any(c not in "0123456789abcdef" for c in args.nro_sha256)):
        parser.error("--nro-sha256 must be 64 lowercase hexadecimal characters")
    report = (verify_package(args.root, args.baseline, args.out) if args.verify else
              package(args.root, args.baseline, args.out, args.version, args.nro_sha256))
    if not report["ok"]:
        raise SystemExit("FAIL: see source-tree-equivalence.json")
    print("PASS: exact source ZIP and baseline reconstruction; source files:",
          report["source_to_manifest"]["actual_files"], "; RMS package files:", len(report["rms_package_files"]))


if __name__ == "__main__":
    main()
