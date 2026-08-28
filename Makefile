# Stage 11: one CMake generation, two product binaries.
# Nested leftover project(a2m)/project(c64m) is not the CI entry.

CMAKE ?= cmake
BUILD_TYPE ?= Debug
JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
BUILD ?= build

.PHONY: help configure build test a2m c64m clean

help:
	@echo "Stage 11 root two-target helper."
	@echo "  make configure  # cmake -B $(BUILD) -S ."
	@echo "  make build      # build a2m + c64m + tests"
	@echo "  make test       # build + ctest (default)"
	@echo "  make a2m / c64m # ctest -L a2m or -L c64m"
	@echo "  make clean      # rm -rf $(BUILD)/"
	@echo "Binaries: ./$(BUILD)/a2m  ./$(BUILD)/c64m  ./$(BUILD)/am65"
	@echo "c64m SKIP (CTest 77) without leftover assets/ is not a failure."

all: test

configure:
	$(CMAKE) -B $(BUILD) -S . -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	$(CMAKE) --build $(BUILD) -j$(JOBS)

test: build
	ctest --test-dir $(BUILD) --output-on-failure

a2m: build
	ctest --test-dir $(BUILD) -L a2m --output-on-failure

c64m: build
	ctest --test-dir $(BUILD) -L c64m --output-on-failure

clean:
	rm -rf $(BUILD)/
