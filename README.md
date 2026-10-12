# MITH.xyz — MITH Doom 0.1.1

A playable, silent Doom FFGL source for Resolume Arena/Avenue by [MITH.xyz](https://MITH.xyz). Native builds for macOS (Apple Silicon + Intel) and Windows x64. Requires Resolume 7.3.1 or newer and OpenGL 4.1. The plugin controls include a MITH.xyz brand field; existing control IDs and the plugin ID `MDOM` are preserved in 0.1.1.

## Downloads

| Platform | Package |
| --- | --- |
| Mac — Apple Silicon + Intel | [MITH Doom 0.1.1 Mac ZIP](downloads/MITH%20Doom%200.1.1%20Mac.zip?raw=1) |
| Windows x64 — preview | [MITH Doom 0.1.1 Windows ZIP](downloads/MITH%20Doom%200.1.1%20Windows.zip?raw=1) |
| Complete source | [MITH Doom 0.1.1 Source ZIP](downloads/MITH%20Doom%200.1.1%20Source.zip?raw=1) |

[SHA-256 checksums](downloads/SHA256SUMS.txt). Each platform ZIP includes installation instructions, complete source and licences. WAD game data is a separate download; instructions are below.

## Play

1. Download and unzip the package for your OS using the links above or this repository's Releases. **Mac:** copy `MITH Doom.bundle` into `Documents/Resolume Arena/Extra Effects/`. **Windows:** copy **both** `MITH Doom.dll` and the complete `MITH Doom Resources` folder there. Avenue uses its own Extra Effects folder. Follow the included **START HERE.txt**.
2. Restart Resolume. Find **MITH Doom** in **Sources** and drag it into a clip slot. Trigger the clip.
3. Choose your original Doom/Doom II IWAD under **Game → WAD File**. The game automatically starts the first level on medium difficulty. Give the initial screen melt about two seconds to finish.
4. Assign keyboard shortcuts using the table below. Use **Piano** mode for all game controls: key down turns a control on, key up turns it off. For these shortcuts, select **This Clip** as the target. Exit shortcut editing to play.

For the free first episode, download the [original Doom95 shareware ZIP](https://www.gamers.org/pub/idgames/idstuff/doom/win95/doom95.zip), extract `DOOM1.WAD`, and select it in the plugin. No old installer needs to run. Game WADs are separate from the plugin downloads. Optionally place the shareware WAD at `Extra Effects/MITH Doom Data/doom1.wad` for automatic selection. Your own full IWAD unlocks its corresponding full game.

## Suggested keys

Choose **Shortcuts → Edit Keyboard**, click the source control, press its key, and enable **Piano** in the shortcut settings. Keys assigned to this game may also have existing Resolume actions; remove conflicting shortcuts in your game composition if needed. The plugin does not capture the keyboard itself.

| Source control | Key |
| --- | --- |
| Forward / Up | W or Up arrow |
| Backward / Down | S or Down arrow |
| Turn Left | Left arrow |
| Turn Right | Right arrow |
| Strafe Left | A |
| Strafe Right | D |
| Fire | Space |
| Use / Open Door | E |
| Run | R |
| Menu / Escape | Escape |
| Enter / Respawn | Enter |
| Automap | Tab |
| Weapons 1–7 | 1–7 |
| Yes / No | Y / N |
| View / Zoom + / − | = / − |

Forward and Backward navigate menus; Turn Left/Right adjust options. Use **Enter / Respawn** to confirm menus or restart after dying. Clicking a game control in the source panel latches it; click again to release it. MIDI/OSC can also drive these controls with 1 on press and 0 on release.

**Game → Restart** resets the game and releases its input controls. **Pause Game** freezes it. **Keep 4:3** corrects Doom's original pixel aspect ratio and adds black bars as needed; disable it to stretch. **Smooth Scaling** switches from crisp nearest-neighbour pixels to linear filtering.

## Behaviour and limits

The original 320×200 software-rendered game is uploaded to an OpenGL texture and displayed at the composition resolution. Engine simulation uses the original 35 Hz timing independently of Resolume's frame rate. It freezes after approximately half a second without a render call. This follows actual host rendering: a source that Arena continues to render offscreen will continue running.

Each source instance gets a separate bundled `DoomEngine` process. WAD loading and game simulation happen outside Arena's render thread. Changing WAD or Restart replaces that process; unloading the source stops it. Engine errors cannot call exit in the Resolume process. Engine config/save files, if created by its built-in menus, live in a disposable temporary directory and disappear on unload/restart.

No audio, persistent saves, mouse look, jumping, multiplayer or modern mod support. Intended for original Doom, Ultimate Doom and Doom II IWADs. Add-on PWADs and Boom/GZDoom-specific assets are outside this version's scope. Doom shareware E1M1 was tested; the entire campaign and full commercial IWADs were not playtested. Multiple instances were tested, though one game clip is easiest to control.

## Verification

`previews/validation.json` records tests through the **actual compiled FFGL bundle** on an Apple M2 Max: game rendering, forward/turn/strafe controls, firing and ammunition HUD change, pause/restart, menu and automap, missing/corrupt WAD recovery, independent instances, rapid reload, 4:3/stretch/smoothing, HD/4K/portrait output, and preservation of host OpenGL state including pixel-upload buffer and stride.

**Mac:** the universal plugin is ad-hoc signed, not Apple-notarized. The compiled FFGL bundle was tested on Apple M2 Max; Intel code was compiled but not run on Intel hardware. Follow START HERE.txt if macOS quarantines an internet download.

**Windows preview:** the x64 DLL and engine compile with statically linked C++ and GLEW dependencies. Binary imports/exports were audited. The Windows process bridge and engine are checked separately under Wine; native Windows Resolume rendering remains unverified because this Mac's Wine runtime cannot create an OpenGL 4.1 context. Windows 10/11 and a GPU driver exposing OpenGL 4.1 are required. The Windows files are not Authenticode-signed.

Live Arena discovery and keyboard-shortcut mapping have not been verified on either platform. The included validation reports distinguish completed checks from remaining host checks.

## Build

For Mac, the Apple Command Line Tools are sufficient. Dependencies are vendored and pinned in `DEPENDENCIES.lock`.

```sh
bash build.sh
python3 tests/test_ffgl.py
```

The test needs Pillow, GPU access and a Doom shareware WAD at `samples/doom1.wad`, or `DOOM_WAD=/absolute/path/to/doom1.wad`. `src/Splash.h` is already generated; Pillow is only required if regenerating that file or running visual tests.

For Windows, install the pinned [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006) toolchain or MinGW-w64 and run:

```sh
MINGW_BIN=/path/to/toolchain/bin bash build-windows.sh
python3 package.py
```

`tests/headless_windows.cpp` verifies the actual Windows process bridge and game frames without a GPU. `tests/smoke_windows.cpp` additionally tests the DLL through an OpenGL 4.1 FFGL host. The headless test executable must sit beside the DLL and Resources folder, and should be linked with `-Wl,--stack,8388608` because it keeps multiple reference framebuffers on its stack.

The GitHub Actions workflow builds both platforms and runs the Windows headless checks. CI status is only confirmed after a workflow run succeeds. `package.py` creates Mac, Windows and Source ZIPs with SHA-256 checksums. Each platform download includes complete source and dependency licences. Game WADs and local development paths are excluded.

Sources: [DoomGeneric](https://github.com/ozkl/doomgeneric), [Resolume FFGL SDK](https://github.com/resolume/ffgl), [GLEW](https://github.com/nigels-com/glew).

GPL-2.0-or-later for the plugin/engine source; dependency notices are in `THIRD_PARTY_NOTICES.md`. Original game assets have their own terms.
