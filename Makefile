# Stage 0 helper: two separate CMake source trees. Not a parent project(machines).
# Do not add_subdirectory both imported project() files into one CMake invocation.

CMAKE ?= cmake
BUILD_TYPE ?= Debug
JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

.PHONY: help configure build test a2m c64m clean

help:
	@echo "Stage 0 dual-prefix helper (not a unified CMake project)."
	@echo "  make configure  # two -S trees: import/a2m and import/c64m"
	@echo "  make build      # build both"
	@echo "  make test       # build + both ctest gates (default)"
	@echo "  make a2m / c64m # one prefix"
	@echo "  make clean      # rm -rf build/"
	@echo "c64m SKIP (CTest 77) without assets/ is not a failure."

all: test

configure:
	$(CMAKE) -B build/a2m  -S import/a2m  -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	$(CMAKE) -B build/c64m -S import/c64m -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	$(CMAKE) --build build/a2m -j$(JOBS)
	$(CMAKE) --build build/c64m -j$(JOBS)

test: build
	ctest --test-dir build/a2m  --output-on-failure
	ctest --test-dir build/c64m --output-on-failure

a2m:
	$(CMAKE) -B build/a2m -S import/a2m -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	$(CMAKE) --build build/a2m -j$(JOBS)
	ctest --test-dir build/a2m --output-on-failure

c64m:
	$(CMAKE) -B build/c64m -S import/c64m -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	$(CMAKE) --build build/c64m -j$(JOBS)
	ctest --test-dir build/c64m --output-on-failure

clean:
	rm -rf build/
