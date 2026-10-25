# PiJava

<img src="assets/icon.png" width="128" alt="PiJava icon">

**An experimental J2ME / MIDP emulator for Nintendo Switch homebrew.**

[中文](README.md) · [Building](docs/BUILDING.md) · [Usage](docs/USAGE.md) · [Status](docs/STATUS.md) · [Contributing](CONTRIBUTING.md)

Current source version: `0.2.114-keyui5`.

PiJava implements its VM and host in C++17 and includes its own Java class library.
Switch is the primary interactive platform. The Linux desktop executable is a
headless diagnostic frontend that writes logs and PNG images.
Compatibility is experimental and must be checked per game and per scenario.
Reaching a title screen does not establish that an entire game works.

This initial public distribution provides source. It contains no commercial games
and does not promise a prebuilt NRO release.

## Features and scope

- Load JARs from `SD:/switch/j2me/games/`.
- MIDlet lifecycle, Canvas / GameCanvas, 2D images and text rendering.
- RMS persistent records and separate J2S5 instant savestates.
- MIDI / WAV paths and FFmpeg based compressed audio decoding.
- Partial M3G and vendor API implementations; coverage is incomplete.
- Per-game input mapping, with up to three Switch inputs per phone key.
- Three Pro controller skins, three older phone skins, and a borderless option.
- Original, Bilinear, Scale2x, LCD, CRT and ScanlinesDX display filters.
- Per-game Native / 30 / 60 speed targets and an optional sampled FPS display.

An API class or method being present does not imply full specification support.
M3G, Mascot Capsule and vendor APIs such as Nokia, Samsung and Siemens remain
incomplete. Fonts, encodings, audio formats and game timing assumptions can also
affect compatibility.

## Build and run

See [BUILDING](docs/BUILDING.md) for requirements and exact commands.
From the repository root:

```sh
# Linux diagnostic executable
python3 -B tools/build.py

# Switch NRO, after installing the toolchain and setting DEVKITPRO
python3 -B tools/build.py --switch
```

Switch requires a legitimate devkitPro / devkitA64 installation, libnx, deko3d
for the default build, and matching Switch portlibs. SDKs are not supplied here.
Linux requires GCC/G++, a JDK capable of producing Java 8 bytecode, and Python.
There is no supported Windows or macOS graphical frontend.

Copy your locally built `build/J2me-vm-0.2.114-keyui5.nro` to
`SD:/switch/j2me/`, then place games you are entitled to use in:

```text
SD:/switch/j2me/games/MyGame.jar
```

Launch the NRO through your Switch homebrew environment.
Only JAR files directly inside `games/` are listed; subdirectories and JARs in
the parent directory are not scanned.

## Switch controls

In the game list, use up/down to select, `A` to start, and `X` to resume an
existing instant savestate. `L / ZL` and `R / ZR` change pages.
Left/right move between the list and settings panel. `B` returns to the list.
In the settings panel, select a row with up/down and press `A` to change it.
Left/right do not change setting values.

The default skin is ivory Pro. Blue and black Pro skins, three phone skins,
and a borderless mode are also available. Controller labels follow the active
mapping. Filters affect presentation, not the Java logical canvas.
The default logical canvas is `240×320`, displayed in a fixed `480×640`
Switch viewport. See [USAGE](docs/USAGE.md) for resolution sidecars.

Press the left stick (`L3`) during gameplay to open the Save / Exit page.
Use `L / R` to switch to the key mapping page. `L3` is reserved for this menu.

To edit a mapping, select a phone key, select one of its three slots, choose a
Switch input, and confirm each step with `A`. Press `+` to persist the draft.
`B` goes back; `X` resets the draft to defaults, which still requires `+` to save.
`Y` switches the draft between per-game and global scope.

The left stick defaults to phone direction keys and is absent from the default
numeric `2 / 4 / 6 / 8` bindings. The D-pad uses phone directions or numeric
`2 / 4 / 6 / 8`, according to the game's direction mode. The right stick retains
numeric `2 / 4 / 6 / 8`; `R3` maps to `0` by default.
See [USAGE](docs/USAGE.md) for the remaining defaults.

## Saves and compatibility

The Save / Exit page can save or delete an instant savestate, or exit the game.
Deletion and exit require a second confirmation. Resume with `X` in the list.

Current savestates use **J2S5**. J2S1 through J2S4 are explicitly rejected.
J2S5 is not a guarantee of compatibility across different builds, class layouts
or JARs. Back up your own data before updates or migration attempts.
RMS is independent: restoring an instant state does not roll back RMS, and
deleting an instant state does not delete RMS.

Native / 30 / 60 are speed targets, not promised frame rates for every game.
The displayed sampled FPS is not necessarily the game's logical FPS.
Game sleep behavior, drawing frequency and device load affect the result.

The upstream keyui5 project completed focused checks and one Linux build and
Switch build. This public export replaces the mapping-page phone illustration
with an original unbranded asset, checked locally. The complete public tree has
not yet been rebuilt for Switch and still requires build validation by users.
These records do not establish full game compatibility or complete physical
Switch acceptance. General error recovery and hard-hang fallback remain
incomplete. See [STATUS](docs/STATUS.md).

## Linux diagnostics

```sh
J2ME_FRAMES=120 ./build/j2mevm \
  --cp romfs/classes --jar MyGame.jar \
  -o build/game.png --log build/game.log
```

There is no interactive game window. `J2ME_FRAMES` counts diagnostic loop
iterations, not universal game frames. Replace `MyGame.jar` with your own file.
Switch production ROMFS excludes example JARs.

## Contributing and license

Include the PiJava version, device, launch method, exact reproduction steps,
JAR SHA-256 and a minimal redacted log in bug reports.
Do not upload commercial JARs, game assets, RMS, savestates, keys or personal data.
Prefer a small MIDlet you wrote yourself. See [CONTRIBUTING](CONTRIBUTING.md).

Project-owned code is [GPL-3.0-or-later](LICENSE). Parts of the Java class library
derive from FreeJ2ME and other projects. Third-party components retain their
original terms and notices; see [THIRD_PARTY](THIRD_PARTY.md) and [licenses/](licenses/).
Check component terms when modifying or redistributing source or binaries.
PiJava is an independent homebrew project, unaffiliated with Nintendo, Oracle
or the phone vendors whose APIs it implements.
