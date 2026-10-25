# Pi Java Emulator / Pi Java模拟器

<img src="assets/icon.png" width="128" alt="Pi Java logo">

An experimental J2ME / MIDP emulator for Nintendo Switch homebrew, written in C++17 with a bundled Java library.

[中文](README.md) · [Download NRO / NSP](https://github.com/incuryou/PiJava/releases/latest) · [Build](docs/BUILDING.md) · [Roadmap](docs/ROADMAP.md)

The current release is **KeyUI6 / GPUF5 / Save1**. Its internal build identifier remains `0.2.114-keyui5-gpuf5-save1`, with NACP version `0.2.114-k5-sv1`. KeyUI6 identifies the later layout revision; binary identifiers have not been relabeled.

Changes include a centered mapping editor with the phone reference image removed, five GPU filters (Bilinear, Scale2x, LCD, CRT, ScanlinesDX), automatic CPU/GPU presentation routing, cross-chapter RMS save transfer with target backups, and the new shared NRO/NSP logo.

The Switch host uses a **three-CPU-core division of work plus GPU presentation/filtering**. The supplied NSP defaults its main thread to CPU 0; two workers are pinned to CPU 1 and CPU 2, with asynchronous logging also using CPU 2. NRO main-thread affinity depends on its loader. Java execution remains cooperatively scheduled in one host executor; this is not three parallel Java interpreters. Filter-off presentation uses the CPU, and GPU filters do not establish full GPU-accelerated 3D game support.

Future work focuses on M3G/Mascot Capsule compatibility, a dedicated GPU 3D backend evaluation, cross-core task coordination, synchronization, load balancing, frame-time stability, memory usage, audio and recovery. These are plans rather than completed features.

Download the supplied NRO and standalone NSP from Releases. No games, user saves, keys or SDK are included. The upload preserves the supplied binary bytes; device installation and full-game acceptance have not been newly performed. The source was compared to NRO resources, while NSP hashes/container/content IDs were checked separately from its supplied strict-validation report.

Production source, build inputs and required resources are included; test classes, reports, logs, screenshots, sample games and private CI are omitted. `romfs/classes/` contains the runtime Java API. Build on Linux with `python3 -B tools/build.py --frozen-java`, or add `--switch` with a configured devkitPro toolchain. See [build details](docs/BUILDING.md).

Project-owned code uses GPL-3.0-or-later. Third-party licenses remain applicable; see [THIRD_PARTY.md](THIRD_PARTY.md) and [dependency sources](docs/DEPENDENCY-SOURCES.md). No affiliation with Nintendo or Nokia is implied.
