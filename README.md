# Image Compression using Seam-Carving
Group assignment for CSD3131 (Algorithm Analysis).

Seam carving (or liquid rescaling) is an algorithm for content-aware image resizing, developed by Shai Avidan and Ariel Shamir. 

As part of a group assignment, we implemented Seam carving using 3 different method:
- Dynamic Programming
- Greedy Algorithm
- Graph Cut (min-cut/max-flow)

<table>
  <tr>
    <td>Group ID</td>
    <td>1</td>

  </tr>
  <tr>
    <td>Presentation Video</td>
    <td>_TODO (YouTube Link)_ </td>
  </tr>
</table>


## Overview

Everything is one program, `seamcarve`, written in C++17 (OpenCV for image I/O, Dear ImGui + GLFW for the window):

| Platform | How it behaves |
|---|---|
| **Linux / WSL** | Command-line tool when you pass arguments (`make run ARGS="..."`). Opens the GUI when you pass `--gui` or no arguments at all (`make gui`) |
| **Windows** | GUI only (`build.bat`), built as a Visual Studio 2026 project. There is no command-line mode |

> **Status:** the project skeleton, build system, CLI and a basic GUI window are in place. The algorithms
> themselves (`energy`, `seam_dp`, `seam_greedy`, `seam_graphcut`, `seam_carver`) are still stubs that report
> "not implemented".

## Building on Linux / WSL (make)

`make` installs the dependencies through [vcpkg](https://github.com/microsoft/vcpkg), so you don't need to
install OpenCV, ImGui or GLFW yourself.

**One-time prerequisites** (needed by vcpkg itself, requires `sudo`):

```bash
sudo apt-get install -y curl zip unzip tar pkg-config build-essential nasm cmake \
    autoconf autoconf-archive automake libtool \
    libxmu-dev libxi-dev libgl-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev
```

The last two lines are for the GUI (vcpkg builds GLFW's X11 libraries from source, which needs the
autotools, and GLFW/OpenGL need X11 and OpenGL development headers). vcpkg prints the
exact list if something is still missing. To show the GUI window from WSL you need WSLg (Windows 11).

Then:

```bash
make
```

The first `make` clones vcpkg into `./vcpkg` and compiles OpenCV, ImGui and GLFW, which can take 15-30+ minutes.
After that, `make` only recompiles when a source file has changed.

> **WSL tip:** keep the repo inside the Linux filesystem (e.g. `~/`), not under `/mnt/c/`.
> Building and deleting files on the Windows drive from WSL is very slow.

### Make commands

| Command | What it does |
|---|---|
| `make` / `make build` | Install dependencies with vcpkg and build the executable |
| `make run ARGS="..."` | Build if needed, then run the CLI with the given arguments (with no `ARGS` it opens the GUI) |
| `make gui [ARGS="-i image.png"]` | Open the interactive ImGui window (same executable; optionally with an image preloaded) |
| `make usage` | Print the command-line usage of the CLI |
| `make test` | Build, then run the tests |
| `make rebuild` | Delete `build/` (keeps vcpkg) and build again |
| `make clean` | Delete everything generated: `build/` and the repo-local `./vcpkg` |
| `make help` | List the commands |

The executable is `build/seamcarve`; everything else (CMake files, object files, the libraries vcpkg
installed) is grouped in `build/cmake/`. On Linux vcpkg links OpenCV, ImGui and GLFW statically, so there are
no extra library files next to the executable. To use your own vcpkg instead of `./vcpkg`, set the
`VCPKG_ROOT` environment variable; `make clean` will leave it alone.

## Building on Windows (build.bat)

`build.bat` generates a **Visual Studio 2026** project with CMake and builds it into a GUI program,
`build\seamcarve.exe`, that you can double-click (or drop an image onto). There is no command-line mode on Windows.

| Command | What it does |
|---|---|
| `build.bat` | Install dependencies with vcpkg, generate the Visual Studio project and build `build\seamcarve.exe` |
| `build.bat run` | Build if needed, then open the app |
| `build.bat open` | Generate the Visual Studio project if needed, then open `build\cmake-windows\SeamCarve.sln` |
| `build.bat clean` | Delete the Windows build output and the repo-local `vcpkg-win` |
| `build.bat help` | List the commands |

- It needs `git` and Visual Studio 2026 with the "Desktop development with C++" workload. The script finds
  Visual Studio itself and uses its CMake (the standalone CMake may be too old to know the VS 2026 generator),
  so any terminal works.
- The first build compiles OpenCV, ImGui and GLFW and takes a while.
- The libraries are DLLs: only the ones the exe needs (OpenCV, GLFW, ...) are copied next to it in `build\`.
  Keep them together if you move the exe.
- If you double-click `build.bat`, the window stays open at the end so you can read the messages.
- Windows files go in `build\cmake-windows\` and `vcpkg-win\`, separate from the Linux `build/cmake/` and
  `vcpkg/`, so the two don't clash. Still, it is best to use a separate clone of the repo for each OS.

### Working in Visual Studio

Open `build\cmake-windows\SeamCarve.sln` (or run `build.bat open`). You code as normal (edit, breakpoints,
F5, Ctrl+Shift+B), but note:

- **CMake owns the project settings.** The `.sln` and `.vcxproj` files are generated; changes made in Visual
  Studio's property pages are lost when CMake regenerates them. Change flags, defines and libraries in `CMakeLists.txt`.
- **New source and header files must be added to `CMakeLists.txt`** (headers are listed there too, which is how they show up in Solution Explorer): algorithm code to `seamcarve_core`, window/CLI code
  to the `seamcarve` executable. Visual Studio then asks to reload the solution.
- **Projects you will see:** `seamcarve` (the program, set as the Startup Project) and `seamcarve_core` (the
  algorithms as a static library). `ALL_BUILD` and `ZERO_CHECK` are generated by CMake and sit in a folder.
- **Debug vs Release:** use Debug for debugging. The Release exe is `build\seamcarve.exe`.
- **No console window:** the Windows program is GUI-only, so `std::cout` output is not shown in a terminal.

## Usage

**CLI (Linux / WSL):**

```bash
make run ARGS="--input <file> --algo <dp|greedy|graphcut> [--energy <gradient|sobel|dc> [--dc-type <recursion|iteration>] [--base-size <px>]] [--width <px>] [--height <px>] [--output <file>] [--verbose]"
```

- At least one of `--width` / `--height` is required (target size, shrink only).
- `--output` is optional; default is `<input>_<algo>.<ext>` next to the input.
- `--energy` chooses how the energy map is computed (default `sobel`). With `--energy dc` (divide and conquer),
  `--dc-type` picks `recursion` (default) or `iteration`, and `--base-size` sets the block size where the splitting
  stops (default 32; at least 3 for recursion). Both only apply together with `--energy dc`.

Example:

```bash
make run ARGS="-i data/input/castle.png -a dp -w 400 -o data/output/castle_dp.png"
```

**GUI (Linux / WSL: `make gui`; Windows: `build.bat run` or double-click `build\seamcarve.exe`):**
type an image path and press Load (or drag and drop an image file onto the window; on Windows only, since WSL does
not pass drops from Windows apps; in WSL you can type or paste a Windows path such as `C:\Users\me\cat.png` and it is
opened as `/mnt/c/Users/me/cat.png`), pick an algorithm and an energy method (Gradient, Sobel or Divide and
conquer; for divide and conquer also its type, recursion or iteration, and block size), set the target size with the Scale % / Width / Height
sliders ("Keep aspect ratio" moves them together, Reset goes back to the original size) and press Carve. Save
result writes the carved image next to the original as `<name>_carved_<width>x<height>.<ext>`. The original and
the result are shown in their own windows at the same scale ("Fit images to window" scales them to fit, untick it
for 100% with scrollbars). Messages appear in the status bar at the bottom (hover a message for the full path). The
window uses the system UI font (Segoe UI on Windows, Ubuntu / DejaVu on Linux) and scales with the display scale
setting. The windows are dockable: drag one by its
title bar onto another to dock it (or to tab it), and drag the dividers to resize. The layout is saved to
`imgui.ini` in the folder you launched from (git-ignored).

## Project structure

CMake builds two things: `seamcarve_core`, a static library holding all the algorithms, and the `seamcarve`
executable (CLI + GUI) that links it.

| CMake setting | Meaning |
|---|---|
| `SEAMCARVE_BUILD_GUI` (ON by default) | Compile the ImGui/GLFW window into `seamcarve`. Without it, `--gui` reports "this build has no GUI" |
| `SEAMCARVE_GUI_ONLY` (OFF by default) | No command-line mode; `main()` always opens the window and, with MSVC, no console window. Used by `build.bat` |

| CMake preset | Used by |
|---|---|
| `default` | CI on Ubuntu: system OpenCV (`libopencv-dev`), GUI off |
| `vcpkg` | `make` on Linux/WSL and CI on Windows: vcpkg dependencies, CLI + GUI |
| `windows-gui` | `build.bat`: Visual Studio 2026 project, GUI-only, DLLs |

## Layout

```
include/seamcarve/   public headers (interfaces)
src/                 energy, seam_dp, seam_greedy, seam_graphcut, seam_carver, types (algorithms);
                     cli, gui, gui_theme (GUI colours/fonts/style), main (program)
tests/               CTest tests (they drive the CLI)
Makefile             build/run commands for Linux/WSL (wraps CMake + vcpkg)
build.bat            build script for the Windows GUI app (Visual Studio 2026 project)
CMakePresets.json    build presets (default, vcpkg, windows-gui)
vcpkg.json           dependencies: OpenCV, ImGui, GLFW
data/input|output    sample images / results
docs/                AI usage log, report notes
.github/workflows/   CI (Ubuntu + Windows)
```

Generated and git-ignored: `build/`, `vcpkg/` (Linux), `vcpkg-win/` (Windows).

## Graph cut

`FindSeamGraphCut` (`src/seam_graphcut.cpp`) turns the energy map into a flow network, runs max-flow
(`FlowGraph` in `src/max_flow.cpp`) and reads the seam from the minimum cut. The helpers are declared in
`include/seamcarve/seam_graphcut.hpp`.

| Edge | Capacity | Purpose |
|---|---|---|
| source → every pixel in the first column | INF | the cut starts left of the image |
| pixel → right neighbour (→ sink for the last column) | pixel energy | cutting it removes that pixel |
| pixel → left neighbour | INF | the cut crosses each row exactly once |
| pixel → left neighbours in the rows above and below | INF | the seam moves at most one column per row |

Every finite cut therefore removes one connected vertical seam and costs exactly its energy, so the minimum
cut is the same optimal seam DP finds. After `MaxFlow`, `MinCut` marks the pixels the source can still reach;
in each row the last of these is the seam pixel (`ReadSeamFromCut`). For a `rows x cols` map the graph has
`rows*cols + 2` nodes and `2*rows*cols + 2*(rows-1)*(cols-1)` edges (plus one reverse edge each).

Tests: `seam_graphcut` checks the graph, the seam read-back and the seams found (against hand-made maps and
a brute-force search over every seam on small random maps); `graphcut_vs_dp` checks that graph cut and DP seams
cost the same, and is reported as skipped until `FindSeamDP` is implemented.

## Continuous integration

`.github/workflows/ci.yml` runs on every push to `main` and every pull request, with two jobs:

- **ubuntu:** installs OpenCV with `apt`, then configures, builds and tests with the `default` preset (GUI off).
- **windows:** installs OpenCV, ImGui and GLFW with vcpkg (cached), then configures, builds and tests with the
  `vcpkg` preset (CLI + GUI).

Both run the CTest tests in `tests/`. CI does not run `make` or `build.bat`.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the team workflow and coding conventions, and
[docs/ai-usage.md](docs/ai-usage.md) for the AI declaration log.
