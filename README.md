# machines

Two emulators for people who enjoy writing software for old computers, or just
exploring how they work:

- **a2m** — Apple ][+ / //e Enhanced — [more about a2m](doc/README-A2M.md)
- **c64m** — Commodore 64 — [more about c64m](doc/README-C64.md)

These started as two separate projects. I wanted to learn one set of controls
and feel at home in either emulator, so I brought their interfaces and debugging
tools together. They still run as separate applications, with a familiar layout
and tools adapted to the machines they emulate.

I built these emulators for my own use, especially for writing and debugging
software. Both have a built-in assembler, and features such as HostFS let me
work with files on my computer without restarting the emulator. When something
goes wrong, the Inspector lets me look back through execution, and Forensics
provides detailed logs to help me track down the bug.

You can see those tools in action in this
[video about the development features](https://www.youtube.com/watch?v=q9SGle1TDpM).
If you enjoy tinkering with these machines too, I hope you'll find something
useful here.

## Getting started

Both emulators are written in C99 and build on macOS, Linux, and Windows. You'll
need CMake **3.24+**, SDL2, and Python 3 (used to generate the built-in help).

On macOS, install the dependencies with `brew install cmake sdl2` and make sure
Python 3 is available. On Ubuntu/Debian, install `build-essential`, `cmake`,
`libsdl2-dev`, and `python3`.

From the repository root, build both emulators:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

Then launch whichever machine you feel like using:

```bash
./build/a2m
./build/c64m
```

In either emulator, **F9** toggles the debugger layout and **Alt+H** opens the
built-in manual. For help with loading software, using the assembler, or finding
your way around the debugger, see the [a2m manual](manual/a2m/manual.md) or
[c64m manual](manual/c64m/manual.md). The individual READMEs linked above cover
machine-specific features and setup.

The build also includes `./build/am65`, the command-line assembler. Run any of
these programs with `--help` to see its command-line options.

### Building on Windows

Use a Visual Studio developer shell with MSVC C11 atomics support, CMake,
Python 3, and SDL2. If you've installed SDL2 through vcpkg, point CMake at your
vcpkg toolchain file:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build -j
.\build\a2m.exe
.\build\c64m.exe
```

Use a separate build directory when switching compilers or operating systems.
With a Visual Studio generator, pass `--config Debug` to the build and `-C Debug`
to CTest; executables are under `build/Debug/`.

## Making a release package

From the repository root:

```bash
python3 tools/package.py
```

`make package` does the same thing. This builds Release copies of a2m, c64m,
and am65, then writes one folder per emulator under `dist/`. Each folder is the
download layout: the program, am65, the ini example, the license, the manual,
samples, symbols, and SDL. The c64m folder also contains `roms/`.

Windows writes a zip. Run the script from a Visual Studio developer shell. If
SDL2 comes from vcpkg, pass the toolchain through:

```powershell
python3 tools/package.py --cmake-args -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

Linux writes a `.tar.gz` and needs `patchelf`. Build it on an older distro
(Ubuntu 22.04 is a solid floor) so the binary still runs on newer ones.

macOS writes a zip you can run locally. To sign with your Developer ID and
make the DMG:

```bash
python3 tools/package.py --sign
```

Notarization runs when the notary credentials are already in the environment
(`APPLE_NOTARY_PROFILE`, or the API key trio, or Apple ID plus app password).
The script does not upload the archives.

## Agent access through MCP

The optional [Machines MCP adapter](mcp/README.md) lets Codex attach to a running
emulator or launch one, discover typed tools, and inspect CPU/memory, screenshots,
recorded evidence and disassembly. It supports both existing control protocols
without changing the emulators. Installation requires Python 3.11 or newer and
is separate from the CMake build.

See the adapter guide for one-time Codex setup, file roots, process ownership,
recording requirements and current limitations. During parallel agent work,
designate one agent to own emulator interaction while others analyze captured data.

## Running the tests

To run the tests after building:

```bash
ctest --test-dir build --output-on-failure
```

You can also use `make test` from the repository root, or run just one emulator's
tests:

```bash
ctest --test-dir build -L a2m
ctest --test-dir build -L c64m
```

Some C64 tests need local files under `assets/` that aren't included in the
repository. CTest reports those as skipped when the files are missing. Known
test failures and verification notes are recorded in the
[development notes](agents/README.md#verification).

## Finding your way around the source

If you'd like to explore the code, the shared interface and debugging tools live
in `src/shell/`, while each machine's emulation lives in its own directory.
Everything builds from the repository root.

| Path | What's there |
|------|--------------|
| `src/shell/` | Shared interface, debugging tools, and utilities |
| `src/apple2/` | Apple II emulation and machine-specific tools |
| `src/c64/` | Commodore 64 emulation and machine-specific tools |
| `src/shell/tools/am65/` | The assembler, shared by both emulators |
| `manual/a2m/`, `manual/c64m/` | Manuals, also used for the built-in help |
| `samples/apple2/` | Apple II programs to try and learn from |
| `tests/` | Shared and machine-specific tests |
| `mcp/` | Optional Python MCP adapter, agent tools, documentation and tests |
| `external/` | Third-party libraries |

For work on the code, start with the [development notes](agents/README.md).
They describe how the shared tools and machine-specific code fit together, and
include instructions for coding agents.

## A new home for the projects

This repository brings together [a2m](https://github.com/StewBC/a2m),
[c64m](https://github.com/StewBC/c64m), and the
[am65 assembler](https://github.com/StewBC/am65). Their earlier repositories
are archived; ongoing development happens here in **machines**.

## License

The emulators are released into the public domain under the Unlicense.
Third-party files under `external/` keep their own licenses. See
[LICENSE](LICENSE) for details.
