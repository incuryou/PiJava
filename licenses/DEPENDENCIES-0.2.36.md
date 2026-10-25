# 0.2.36 dependency provenance

## Runtime / build separation
- Core C++ and Java sources, regression harnesses, restricted JIT emitter, Nokia OTT parser and compatibility edits are included in the source archive.
- Existing FreeJ2ME-Plus-derived M3G code keeps its original notices and `FreeJ2ME-plus-GPL-3.0.txt` / provenance file. No new MascotME code was imported. Public project documentation was reviewed, not presented as our testing result.
- Nokia API documentation and FreeJ2ME-Plus Samsung AudioClip behavior were consulted; the new adapters use this VM's own pixels and Player backend. Unimplemented MMF/SMAF is explicitly rejected.
- FFmpeg public headers came from the user-provided SDK recovery, package `switch-ffmpeg 7.1-5`. Exact per-header checksums are in `third_party/ffmpeg_headers/MANIFEST.json`. The matching SDK package metadata labels its configured library GPL. Do not describe the binary dependency as LGPL-only.
- Desktop uses matching avcodec61 / avformat61 / avutil59 dynamic libraries when present. Switch links the SDK's static FFmpeg libraries and their build dependencies (including zlib, bzip2, dav1d and platform libraries).
- This archive does NOT contain the implementation sources of all linked toolchain/codec dependencies and is not labeled a self-contained corresponding-source distribution of those external projects. Obtain the exact package recipes and sources when rebuilding or redistributing with them; preserve all upstream obligations and notices. No legal conclusion or relicensing of the user's original project is asserted here.

## Upstream source and recipes
- FFmpeg 7.1 full upstream source: https://ffmpeg.org/releases/ffmpeg-7.1.tar.xz
- FFmpeg build/legal notes: https://ffmpeg.org/legal.html
- devkitPro portlibs recipes: https://github.com/devkitPro/pacman-packages
- libnx: https://github.com/switchbrew/libnx
- deko3d: https://github.com/devkitPro/deko3d
- dav1d: https://code.videolan.org/videolan/dav1d
- FreeJ2ME-Plus: https://github.com/TASEmulators/freej2me-plus
- MascotME reference only, NOT integrated: https://github.com/rmn20/MascotME

## Excluded assets
Unchanged font dependencies are listed in assets-reuse.json and intentionally omitted. Reuse the verified original source archive through tools/restore_assets.py. The NRO embeds the application resources required at runtime. Commercial collection JARs and extracted commercial audio fixtures are not included. Testing screenshots are separately marked as observations of user-provided games.
