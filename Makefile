# Convenience wrapper around CMake + vcpkg (Linux / WSL). Run `make help` from the repo root.
#
# Everything is one program, build/seamcarve: `make run ARGS="..."` runs it as a command-line
# tool, `make gui` (or running it with no arguments) opens the ImGui window.
#
# `make` clones and bootstraps vcpkg into ./vcpkg (once), lets vcpkg build OpenCV, ImGui and GLFW
# from vcpkg.json (once, slow), configures CMake (once) and compiles the sources. Later runs
# only rebuild when a source file changed.
#
# vcpkg itself needs a few system tools; if it complains, install them with:
#   sudo apt install build-essential curl zip unzip tar pkg-config nasm cmake \
#       autoconf autoconf-archive automake libtool \
#       libxmu-dev libxi-dev libgl-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev
PRESET       := vcpkg
BUILD_DIR    := build/cmake
EXE          := build/seamcarve
# The generated Makefile only exists after a configure that succeeded (CMakeCache.txt can be left
# behind by a failed one).
CACHE        := $(BUILD_DIR)/Makefile
BENCH        := build/seamcarve_bench
SOURCES      := $(wildcard src/*.cpp include/seamcarve/*.hpp bench/*.cpp) CMakeLists.txt bench/CMakeLists.txt

# vcpkg location, relative to the repo root. Set VCPKG_DIR (or the VCPKG_ROOT environment
# variable) to use your own vcpkg instead of the repo-local ./vcpkg.
ifndef VCPKG_DIR
VCPKG_DIR    := $(or $(VCPKG_ROOT),vcpkg)
endif
# The "vcpkg" CMake preset reads $VCPKG_ROOT, which CMake needs as an absolute path.
export VCPKG_ROOT := $(abspath $(VCPKG_DIR))

.DEFAULT_GOAL := build
.PHONY: help build test bench run usage gui clean rebuild

help: ## Show this help
	@echo "Usage: make [target]"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*## "} {printf "  %-10s %s\n", $$1, $$2}'
	@echo ""
	@echo "CLI example: make run ARGS=\"-i data/input/castle.png -a dp -w 400 -o data/output/castle_dp.png\""
	@echo "GUI example: make gui ARGS=\"-i data/input/castle.png\""
	@echo "Benchmarks:  make bench ARGS=\"--benchmark_filter=FindSeam\""

# Fetch and bootstrap vcpkg (only if it is not there yet).
$(VCPKG_DIR)/vcpkg:
	test -d $(VCPKG_DIR) || git clone --depth 1 https://github.com/microsoft/vcpkg.git $(VCPKG_DIR)
	$(VCPKG_DIR)/bootstrap-vcpkg.sh -disableMetrics

# Configure CMake; in vcpkg manifest mode this also installs OpenCV, ImGui and GLFW (only if not
# configured yet).
$(CACHE): $(VCPKG_DIR)/vcpkg
	cmake --preset $(PRESET)

# Compile (only if a source file is newer than the executable).
$(EXE): $(CACHE) $(SOURCES)
	cmake --build --preset $(PRESET)

build: $(EXE) ## Install deps via vcpkg and build the executable (default)

test: $(EXE) ## Build, then run the CTest tests
	ctest --preset $(PRESET)

bench: $(EXE) ## Build, then run the benchmarks: make bench [ARGS="--benchmark_filter=dp"]
	$(BENCH) $(ARGS)

run: $(EXE) ## Run the CLI: make run ARGS="-i in.png -a dp -w 400" (no ARGS opens the GUI)
	$(EXE) $(ARGS)

usage: $(EXE) ## Print the seamcarve command-line usage
	$(EXE) --help

gui: $(EXE) ## Open the ImGui window: make gui ARGS="-i data/input/castle.png" (image optional)
	$(EXE) --gui $(ARGS)

clean: ## Delete everything generated: build/ and the repo-local vcpkg (with its installed libs)
	rm -rf build
ifeq ($(VCPKG_DIR),vcpkg)
	rm -rf vcpkg
else
	@echo "Keeping $(VCPKG_DIR) (your own vcpkg, not the repo-local one)"
endif

rebuild: ## Delete build/ only (keeps vcpkg), then build again
	rm -rf build
	$(MAKE) build
