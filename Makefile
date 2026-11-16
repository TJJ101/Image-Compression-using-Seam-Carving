# Convenience wrapper around CMake + vcpkg (Linux / WSL). Run `make help` from the repo root.
#
# `make` clones and bootstraps vcpkg into ./vcpkg (once), lets vcpkg build OpenCV from
# vcpkg.json (once, slow), configures CMake (once) and compiles the sources. Later runs only
# rebuild when a source file changed.
#
# vcpkg itself needs a few system tools; if it complains, install them with:
#   sudo apt install build-essential curl zip unzip tar pkg-config nasm
PRESET       := vcpkg
BUILD_DIR    := build/cmake
EXE          := build/seamcarve
CACHE        := $(BUILD_DIR)/CMakeCache.txt
SOURCES      := $(wildcard src/*.cpp include/seamcarve/*.hpp) CMakeLists.txt

# vcpkg location, relative to the repo root. Set VCPKG_DIR (or the VCPKG_ROOT environment
# variable) to use your own vcpkg instead of the repo-local ./vcpkg.
ifndef VCPKG_DIR
VCPKG_DIR    := $(or $(VCPKG_ROOT),vcpkg)
endif
# The "vcpkg" CMake preset reads $VCPKG_ROOT, which CMake needs as an absolute path.
export VCPKG_ROOT := $(abspath $(VCPKG_DIR))

.DEFAULT_GOAL := build
.PHONY: help build test run usage clean rebuild

help: ## Show this help
	@echo "Usage: make [target]"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*## "} {printf "  %-10s %s\n", $$1, $$2}'
	@echo ""
	@echo "Example: make run ARGS=\"-i data/input/castle.png -a dp -w 400 -o data/output/castle_dp.png\""

# Fetch and bootstrap vcpkg (only if it is not there yet).
$(VCPKG_DIR)/vcpkg:
	test -d $(VCPKG_DIR) || git clone --depth 1 https://github.com/microsoft/vcpkg.git $(VCPKG_DIR)
	$(VCPKG_DIR)/bootstrap-vcpkg.sh -disableMetrics

# Configure CMake; in vcpkg manifest mode this also installs OpenCV (only if not configured yet).
$(CACHE): $(VCPKG_DIR)/vcpkg
	cmake --preset $(PRESET)

# Compile (only if a source file is newer than the executable).
$(EXE): $(CACHE) $(SOURCES)
	cmake --build --preset $(PRESET)

build: $(EXE) ## Install deps via vcpkg and build the executable (default)

test: $(EXE) ## Build, then run the CTest tests
	ctest --preset $(PRESET)

run: $(EXE) ## Run the CLI: make run ARGS="-i in.png -a dp -w 400"
	$(EXE) $(ARGS)

usage: $(EXE) ## Print the seamcarve command-line usage
	$(EXE) --help

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
