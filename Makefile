# Stage 4 helper: two separate CMake source trees. Not a parent project(machines).
# Each nested project() add_subdirectorys src/shell + repo-root external/.
# Do not add_subdirectory both nested project() files into one CMake invocation.

CMAKE ?= cmake
BUILD_TYPE ?= Debug
JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

.PHONY: help configure build test a2m c64m clean

help:
	@echo "Stage 4 dual-tree helper (not a unified CMake project)."
	@echo "  make configure  # two -S trees: src/machine/apple2 and src/machine/c64"
	@echo "  make build      # build both"
	@echo "  make test       # build + both ctest gates (default)"
	@echo "  make a2m / c64m # one tree"
	@echo "  make clean      # rm -rf build/"
	@echo "c64m SKIP (CTest 77) without assets/ is not a failure."

all: test

configure:
	$(CMAKE) -B build/a2m  -S src/machine/apple2  -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	$(CMAKE) -B build/c64m -S src/machine/c64    -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	$(CMAKE) --build build/a2m -j$(JOBS)
	$(CMAKE) --build build/c64m -j$(JOBS)

test: build
	ctest --test-dir build/a2m  --output-on-failure
	ctest --test-dir build/c64m --output-on-failure

a2m:
	$(CMAKE) -B build/a2m -S src/machine/apple2 -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	$(CMAKE) --build build/a2m -j$(JOBS)
	ctest --test-dir build/a2m --output-on-failure

c64m:
	$(CMAKE) -B build/c64m -S src/machine/c64 -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	$(CMAKE) --build build/c64m -j$(JOBS)
	ctest --test-dir build/c64m --output-on-failure

clean:
	rm -rf build/
