# Complete source packaging

Run after the source tree is frozen. Prepare the original 0.2.42 baseline with
its three original fonts; never substitute donor code. Supply the reused NRO's
SHA256 when generating the bundle:

```sh
python3 tools/package_source.py --root . --baseline ../j2me-0.2.42-s1-src \
  --out ../source-delivery --version 0.2.43-p1-r01 \
  --nro-sha256 <64-lowercase-hex-digest>
python3 tools/package_source.py --verify --root . \
  --baseline ../j2me-0.2.42-s1-src --out ../source-delivery
python3 tools/test_package_source.py
```

The output directory must be outside both trees. Python 3 and a GNU-compatible
`patch` executable are required. Paths are command-line inputs; no prior session
paths are embedded. Generation updates the project's `SOURCE-MANIFEST.json`.
Verification updates only `source-tree-equivalence.json` and
`patch-reconstruction.log` in the delivery directory.

Source inventory excludes only root `build/`, root `rms/`, `.git/` and
`__pycache__/` cache directories, plus the manifest's own entry. It preserves
`java/javax/microedition/rms/`, `romfs/classes/javax/microedition/rms/`, and every
other nested `rms/` directory. No filename extension or Java package name is
excluded. Symlinks fail explicitly rather than being silently omitted.

Outputs are `full-source.zip`, `baseline-to-p1.patch`, `binary-files.zip`,
`source-changes.json`, `binary-changes.json`, `deleted-files.json`,
`baseline-manifest.json`, `source-tree-equivalence.json`, and
`patch-reconstruction.log`. The ZIP contains the manifest too. Every changed
path has an addition/modification/deletion declaration with before/after hashes
and sizes. UTF-8 text with LF uses a standard unified diff, including missing
final newline markers. Binary data, CR line endings, and added empty files use
the exact-byte overlay; every deletion is explicitly listed.

Reconstruction starts with the prepared baseline source inventory and its own
manifest, applies the text patch with zero fuzz, extracts `binary-files.zip`, and
removes `deleted-files.json` paths. Verification compares the entire resulting
file set and bytes against the full ZIP extraction. Neither extracted tree is
filtered, so unexpected output directories or cache files are detected. It also
compares the source inventory in both directions against ZIP contents and the
manifest, then independently recomputes every change declaration, patch byte,
and binary overlay entry. Only the manifest's self-entry is exempt from the
manifest comparison. This is separate from a clean full build and NRO/ROMFS
verification, which the release closeout must also perform.

`SHA256SUMS`, build provenance, and the final return archive should be generated
after all release evidence is finalized; this source tool does not hash a
partially assembled return directory.
