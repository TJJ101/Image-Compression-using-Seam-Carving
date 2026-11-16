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


## Building

The project is built on Linux / WSL with `make` from the repo root. `make` installs the
dependencies through [vcpkg](https://github.com/microsoft/vcpkg), so you don't need to install
OpenCV yourself.

**One-time prerequisites** (needed by vcpkg itself, requires `sudo`):

```bash
sudo apt-get install -y curl zip unzip tar pkg-config build-essential nasm cmake
```

Then:

```bash
make
```

The first `make` clones vcpkg into `./vcpkg` and compiles OpenCV, which can take 15-30+ minutes.
After that, `make` only recompiles when a source file has changed.

> **WSL tip:** keep the repo inside the Linux filesystem (e.g. `~/`), not under `/mnt/c/`.
> Building and deleting files on the Windows drive from WSL is very slow.

### Make commands

| Command | What it does |
|---|---|
| `make` / `make build` | Install dependencies with vcpkg and build the executable |
| `make run ARGS="..."` | Build if needed, then run the CLI with the given arguments |
| `make usage` | Print the command-line usage of the CLI |
| `make test` | Build, then run the tests |
| `make rebuild` | Delete `build/` (keeps vcpkg) and build again |
| `make clean` | Delete everything generated: `build/` and the repo-local `./vcpkg` |
| `make help` | List the commands |

The executable is `build/seamcarve`; everything else (CMake files, object files, the libraries vcpkg
installed) is grouped in `build/cmake/`. To use your own vcpkg instead of `./vcpkg`, set the
`VCPKG_ROOT` environment variable; `make clean` will leave it alone.

## Usage

```bash
make run ARGS="--input <file> --algo <dp|greedy|graphcut> [--width <px>] [--height <px>] [--output <file>] [--verbose]"
```

- At least one of `--width` / `--height` is required (target size, shrink only).
- `--output` is optional; default is `<input>_<algo>.<ext>` next to the input.

Example:

```bash
make run ARGS="-i data/input/castle.png -a dp -w 400 -o data/output/castle_dp.png"
```

## Layout

```
include/seamcarve/   public headers (interfaces)
src/                 energy, seam_dp, seam_greedy, seam_graphcut, seam_carver, cli, main
tests/               CTest tests
Makefile             build/run commands (wraps CMake + vcpkg)
data/input|output    sample images / results
docs/                AI usage log, report notes
.github/workflows/   CI (Ubuntu + Windows)
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for the team workflow and [docs/ai-usage.md](docs/ai-usage.md) for the AI declaration log.
