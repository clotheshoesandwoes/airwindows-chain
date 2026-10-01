<p align="center">
  <img src="docs/chain.png" width="900" alt="Airwindows Chain: six effects in the chain on the left with a level meter after each, ToTape8 and its faders on the right, Chris's write-up underneath">
</p>

<h1 align="center">Airwindows Chain</h1>

<p align="center">
  Up to sixteen Airwindows effects in one plugin, in any order,<br>
  with Chris Johnson's own notes next to every control.
</p>

<p align="center">
  <a href="https://github.com/clotheshoesandwoes/airwindows-chain/releases/latest"><b>Download for Windows</b></a>
  &nbsp;·&nbsp; <a href="https://seankani.com/airwindows-chain/">Website</a>
  &nbsp;·&nbsp; <a href="#using-it">Using it</a>
  &nbsp;·&nbsp; <a href="#build-it-yourself">Build it yourself</a>
</p>

<p align="center">Free. VST3 and CLAP, Windows 10 or 11, 64-bit. The code is MIT.</p>

<br>

## What it does

- **One chain, one window.** Eight slots ready, sixteen if you need them. Drag to reorder, bypass or blend each effect, and watch the level after every one. Undo covers all of it.
- **Every effect, with the notes.** All 504 effects from [Airwindows Consolidated](https://www.airwindows.com/consolidated/), unchanged. The browser has Chris's recommended list, every category, search, favourites, and his Airwindopedia write-up for whatever you hover.
- **His numbers, not mine.** Controls show the values the effect itself reports. Faders by default, knobs if you prefer them. Ctrl or Shift for fine moves.
- **Nothing touches the audio code.** The chain is tested against the effects run on their own: the output matches, bit for bit, with every one of the 504 run through a slot.

<br>

<p align="center">
  <img src="docs/browse.png" width="900" alt="The browser: Chris recommends and every category on the left, effects with one-line descriptions in the middle, the write-up for ClearCoat on the right">
</p>
<p align="center"><sub>The browser. Chris recommends, then every category, then the write-up for whatever you are on.</sub></p>

<br>

<p align="center">
  <img src="docs/browser-vocals.png" width="900" alt="The browser with the For vocals switch on: Chris recommends narrowed to 66, every category showing only its vocal effects">
</p>
<p align="center"><sub>The For vocals switch on: the same lists, narrowed to what suits a voice.</sub></p>

<br>

## Download

Windows, 64-bit: [latest release](https://github.com/clotheshoesandwoes/airwindows-chain/releases/latest).

- `AirwindowsChain-<version>-setup.exe` installs the VST3 and CLAP where every DAW looks, and adds an uninstaller.
- `AirwindowsChain-<version>-windows.zip` if you'd rather copy the files yourself: the `.vst3` folder goes in `C:\Program Files\Common Files\VST3`, the `.clap` in `C:\Program Files\Common Files\CLAP`.

Windows warns that the publisher is unknown the first time; that is what an unsigned program from one person looks like. Then rescan plugins in your DAW once. It is listed under Kani.

<br>

## Using it

| Where | What |
|---|---|
| Chain list | Eight slots on show, more as they fill, up to sixteen. Click an empty one to add. Click to select. Drag to reorder. The switch bypasses. Double-click to replace. |
| Add effect | Opens the browser. Type to search names, categories and descriptions. Hover to read, click or Enter to add. Shift-click adds and keeps the browser open. Esc closes. |
| Type to search | With the mouse over the window, type a letter and the browser opens with it. Keys the plugin doesn't use go to the host. Can be turned off in the chain menu. |
| For vocals | A switch under All effects in the browser. On, every list, the categories and search included, shows only the effects that suit a voice: compressors and gates, de-essers, air, channel strips, tape, plates and rooms, doublers. The list is mine, not Chris's, and lives in `src/VocalList.h`. |
| Effect panel | Drag a control, or click a fader's track to jump. Hold Ctrl or Shift while dragging for ten times finer moves. Double-click a control to reset it, or its value to type one. The arrows step through the effect's category. |
| Knobs | "Knobs instead of faders" in the chain menu lays the controls out as a grid of knobs. Drag up or right to raise. |
| Mix | Blends each effect with what went into it. |
| Meters | The small meter at the right edge of each effect is the level after it, from -60 dB to full scale. Red means it went over. Input and Output have one too. |
| Star | Marks a favourite. Favourites get their own list in the browser, first in line. |
| Right-click a row | Replace, duplicate, add an effect after this one, bypass, move, remove. |
| Undo | Next to the chain name after any edit. Covers add, remove, replace, move, duplicate, clear and open, forty steps deep. |
| Chain name | Save and open chains. They are small files in `Documents\Airwindows Chain`. Each saved chain in the menu shows the effects it holds, so you know what you are opening. Also holds Undo, the theme and accent choices, and About. |
| Theme | Warm, Cool, Black or Light, with an amber, coral, mint, sky, lilac or plain accent. Remembered for every instance. |

The host sees sixteen fixed blocks of parameters, named after whatever sits in each slot ("2. Density2: Drive"). Automation follows an effect when you reorder the chain.

<br>

<p align="center">
  <img src="docs/browser.png" width="900" alt="Typing tape from the main window: the browser filtered to tape effects, with the write-up for the highlighted one">
</p>
<p align="center"><sub>Typing "tape" from the main window.</sub></p>

<br>

<p align="center">
  <img src="docs/knobs.png" width="900" alt="The same chain with ToTape8's controls laid out as a grid of knobs">
</p>
<p align="center"><sub>Knobs, if you prefer them. Same values, same fine control.</sub></p>

<br>

<p align="center">
  <img src="docs/themes.png" width="900" alt="Four looks side by side: Warm with an amber accent, Cool with sky blue, Black with mint, and Light with amber">
</p>
<p align="center"><sub>Warm, Cool, Black and Light, each with six accents. Picked once, kept for every instance.</sub></p>

<br>

## Build it yourself

```bat
tools\build.bat
tools\install.bat
```

`build.bat` needs Visual Studio 2022 with the C++ workload (it finds it through vswhere; CMake and Ninja come with it). The first build compiles all 504 effects and takes a few minutes. `install.bat` copies the result into the plugin folders; Windows asks for admin rights. `tools\package.py` makes the zip and the installer (the installer needs NSIS).

## Tests

`build\awchain_harness_artefacts\Release\awchain_harness.exe` runs everything headless:

| Command | Checks |
|---|---|
| `test` | The chain against the same effects run directly (bit-exact, including mix, bypass, reorder, 44.1/48/96 kHz, odd block sizes, mono). Editing, state and chain files. Four seconds of random edits against a running audio thread. All 504 effects, at defaults and with every control at maximum. Every name on the vocals list exists. |
| `vst3 <path to .vst3>` | Loads the built plugin the way a DAW does: passthrough when empty, project state restore, processing, state save. |
| `snap <folder>` | Renders the editor to PNGs: main view, small and large windows, browser, search, the vocals switch, empty and full chains, every theme, About. |
| `frames <folder> [seconds fps scale]` | Renders the editor frame by frame with audio running and a fader moving, for clips. |
| `dump <file.json>` | Every effect with its category, Chris's one line, its control names and his write-up, as JSON. The website's browser is built from it. |

## How it works

- `src/Catalog.*`: every effect in the registry, the search, the docs, and the vocals list.
- `src/ChainProcessor.*`: the chain. The message thread owns the model. The audio thread owns its own effect instances and hears about changes through a lock-free queue. Replaced instances go back through a second queue and are freed on the message thread, so nothing is allocated or freed while audio runs. Swapping, bypassing and removing fade over 20 ms.
- `src/ChainEditor.cpp`, `src/ui/`: the interface. IBM Plex, drawn by hand, no images.

## Where it comes from

The effects are [Chris Johnson's](https://www.airwindows.com), from Airwindows. He publishes them as open source and writes up each one; those write-ups are what you read inside the plugin. [airwin2rack](https://github.com/baconpaul/airwin2rack), by BaconPaul and the Surge Synth Team, packaged every effect into one plugin, Consolidated, and built the registry this runs on. Airwindows Chain adds the chain, the browser and the interface, and leaves the audio code alone.

If you like the effects, [Chris's Patreon](https://www.patreon.com/airwindows) is what pays for them.

## Licence

The code in this repository is MIT: use it however you like. The effects (Airwindows, Chris Johnson) and the registry that packages them (airwin2rack, BaconPaul and the Surge Synth Team) are MIT as well.

The built plugin also contains JUCE and the VST3 SDK, which are GPLv3 for open-source projects. So the plugin you build or download is GPLv3: free to use and pass on, and anyone who ships it has to share the source too.

Fonts: IBM Plex Sans and Mono, SIL Open Font License (`resources/fonts/OFL.txt`). CLAP support: clap-juce-extensions, MIT.
