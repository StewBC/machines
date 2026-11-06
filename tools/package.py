#!/usr/bin/env python3
"""Stage a2m and c64m release folders and archive them.

Run from any directory:

    python3 tools/package.py
    python3 tools/package.py --sign

The script configures a Release build in build-package/, copies the
published file layout into dist/<product>-<version>/, and writes:

    Windows   dist/<product>-<version>-windows-<arch>.zip
    Linux     dist/<product>-<version>-linux-<arch>.tar.gz
    macOS     dist/<product>.app, then a zip, or with --sign a signed DMG

macOS puts the executable and SDL libraries in Contents/MacOS, and the
manual, samples, symbols, and ROMs in Contents/Resources. Icons come from
packaging/macos/<product>.icns. SDL stays beside the executable: the
Homebrew SDL3 install name is too short to retarget into Frameworks.

c64m's folder includes roms/. Both folders include am65. SDL is copied
next to the executable. On macOS, Homebrew sdl2-compat loads SDL3 by
name at runtime, so libSDL3.dylib is copied as well when the SDL2
library asks for it.

--sign uses the Developer ID Application certificate in the keychain.
Notarization runs only when credentials are already in the environment.
The script does not upload a GitHub release.

Notary environment, one of:

    APPLE_NOTARY_PROFILE
    APPLE_NOTARY_KEY, APPLE_NOTARY_KEY_ID, APPLE_NOTARY_ISSUER
    APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD

Windows: run from a Visual Studio developer shell so dumpbin is on PATH.
Pass a vcpkg toolchain with a repeated --cmake-args if you need one:

    python3 tools/package.py --cmake-args -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

Linux: patchelf must be installed. Build on the oldest glibc you want
to support.
"""

from __future__ import annotations

import argparse
import os
import platform
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import time
import zipfile
from dataclasses import dataclass
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent


@dataclass(frozen=True)
class Product:
    name: str
    version_file: str
    version_macro: str
    exe: str
    ini: str
    manual: str
    samples: str
    symbols: str
    roms: tuple[str, ...]
    extra_dirs: tuple[str, ...]


PRODUCTS = (
    Product(
        "a2m",
        "src/apple2/CMakeLists.txt",
        "A2M_VERSION",
        "a2m",
        "a2m.ini.example",
        "manual/a2m/manual.md",
        "samples/apple2",
        "symbols/apple2",
        (),
        ("samples/apple2/hostfs",),
    ),
    Product(
        "c64m",
        "src/c64/CMakeLists.txt",
        "C64M_VERSION",
        "c64m",
        "c64m.ini.example",
        "manual/c64m/manual.md",
        "samples/c64",
        "symbols/c64",
        ("roms/system.rom", "roms/character.rom", "roms/1541.rom"),
        (),
    ),
)

# Windows import libraries that belong to the OS, not the zip.
WINDOWS_SYSTEM_DLLS = {
    "kernel32.dll",
    "user32.dll",
    "gdi32.dll",
    "advapi32.dll",
    "shell32.dll",
    "ole32.dll",
    "oleaut32.dll",
    "uuid.dll",
    "comdlg32.dll",
    "comctl32.dll",
    "ws2_32.dll",
    "winmm.dll",
    "imm32.dll",
    "version.dll",
    "setupapi.dll",
    "cfgmgr32.dll",
    "bcrypt.dll",
    "ncrypt.dll",
    "crypt32.dll",
    "shlwapi.dll",
    "ntdll.dll",
    "rpcrt4.dll",
    "msvcrt.dll",
    "ucrtbase.dll",
    "dwmapi.dll",
    "uxtheme.dll",
    "opengl32.dll",
    "glu32.dll",
    "dinput8.dll",
    "xinput1_4.dll",
    "winhttp.dll",
    "iphlpapi.dll",
    "hid.dll",
    "mmdevapi.dll",
    "propsys.dll",
}

LINUX_SYSTEM_SONAMES = {
    "libc.so.6",
    "libm.so.6",
    "libdl.so.2",
    "libpthread.so.0",
    "librt.so.1",
    "libresolv.so.2",
    "libutil.so.1",
}


def log(message: str) -> None:
    print(message, flush=True)


def run(cmd: list[str], cwd: Path | None = None) -> None:
    log("+ " + " ".join(cmd))
    subprocess.check_call(cmd, cwd=str(cwd) if cwd else None)


def capture(cmd: list[str]) -> str:
    return subprocess.check_output(cmd, text=True, errors="replace")


def read_version(product: Product) -> str:
    text = (REPO / product.version_file).read_text(encoding="utf-8")
    match = re.search(rf'{re.escape(product.version_macro)}="([^"]+)"', text)
    if not match:
        raise SystemExit(f"could not find {product.version_macro} in {product.version_file}")
    return match.group(1)


def os_tag() -> str:
    system = platform.system()
    machine = platform.machine().lower()
    arch = {
        "amd64": "x64",
        "x86_64": "x64",
        "arm64": "arm64",
        "aarch64": "arm64",
        "x86": "x86",
    }.get(machine, machine)
    if system == "Darwin":
        return f"macos-{arch}"
    if system == "Windows":
        return f"windows-{arch}"
    if system == "Linux":
        return f"linux-{arch}"
    raise SystemExit(f"unsupported operating system {system}")


def tracked_files(prefix: str) -> list[str]:
    out = subprocess.check_output(
        ["git", "ls-files", "-z", "--", prefix],
        cwd=REPO,
    )
    return [entry for entry in out.decode("utf-8", "replace").split("\0") if entry]


def copy_file(src: Path, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dest)


def make_writable(path: Path) -> None:
    mode = path.stat().st_mode
    path.chmod(mode | stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)


def find_exe(build: Path, name: str) -> Path:
    suffix = ".exe" if platform.system() == "Windows" else ""
    candidates = [
        build / f"{name}{suffix}",
        build / name,
        build / "Release" / f"{name}{suffix}",
        build / "RelWithDebInfo" / f"{name}{suffix}",
        build / "MinSizeRel" / f"{name}{suffix}",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise SystemExit(f"did not find {name} under {build}")


def configure_and_build(build: Path, cmake_args: list[str]) -> None:
    cmd = ["cmake", "-S", str(REPO), "-B", str(build)]
    if platform.system() != "Windows" and shutil.which("ninja"):
        cmd.extend(["-G", "Ninja"])
    cmd.append("-DCMAKE_BUILD_TYPE=Release")
    cmd.extend(cmake_args)
    run(cmd)
    jobs = str(os.cpu_count() or 4)
    run([
        "cmake", "--build", str(build),
        "--config", "Release",
        "--parallel", jobs,
        "--target", "a2m", "c64m", "am65",
    ])


def fill_payload(product: Product, build: Path, root: Path, exe_dir: Path) -> None:
    """Copy the published files. root holds data. exe_dir holds the programs."""
    exe_name = product.exe + (".exe" if platform.system() == "Windows" else "")
    am65_name = "am65.exe" if platform.system() == "Windows" else "am65"
    root.mkdir(parents=True, exist_ok=True)
    exe_dir.mkdir(parents=True, exist_ok=True)
    copy_file(find_exe(build, product.exe), exe_dir / exe_name)
    copy_file(find_exe(build, "am65"), exe_dir / am65_name)
    copy_file(REPO / product.ini, root / product.ini)
    copy_file(REPO / "LICENSE", root / "LICENSE")
    copy_file(REPO / product.manual, root / "manual.md")

    for rel in tracked_files(product.samples):
        src = REPO / rel
        if src.is_file():
            copy_file(src, root / rel)
    for extra in product.extra_dirs:
        src = REPO / extra
        if not src.is_dir():
            continue
        shutil.copytree(
            src,
            root / extra,
            dirs_exist_ok=True,
            ignore=shutil.ignore_patterns(".DS_Store"),
        )

    symbol_dir = root / "symbols"
    symbol_dir.mkdir()
    copied_symbol = False
    for rel in tracked_files(product.symbols):
        src = REPO / rel
        if src.is_file():
            copy_file(src, symbol_dir / src.name)
            copied_symbol = True
    if not copied_symbol:
        raise SystemExit(f"no symbol files for {product.name}")

    if product.roms:
        rom_dir = root / "roms"
        rom_dir.mkdir()
        for rel in product.roms:
            src = REPO / rel
            if not src.is_file():
                raise SystemExit(f"missing ROM {rel}")
            copy_file(src, rom_dir / src.name)


def stage_product(product: Product, version: str, build: Path, dist: Path) -> Path:
    stage = dist / f"{product.name}-{version}"
    if stage.exists():
        shutil.rmtree(stage)
    fill_payload(product, build, stage, stage)
    log(f"staged {stage.relative_to(REPO)}")
    return stage


def write_info_plist(path: Path, product: Product, version: str) -> None:
    # CFBundleIconFile is the resource name without .icns.
    path.write_text(
        "\n".join([
            '<?xml version="1.0" encoding="UTF-8"?>',
            '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">',
            '<plist version="1.0">',
            "<dict>",
            "  <key>CFBundleName</key>",
            f"  <string>{product.name}</string>",
            "  <key>CFBundleDisplayName</key>",
            f"  <string>{product.name}</string>",
            "  <key>CFBundleIdentifier</key>",
            f"  <string>com.stefanwessels.{product.name}</string>",
            "  <key>CFBundleVersion</key>",
            f"  <string>{version}</string>",
            "  <key>CFBundleShortVersionString</key>",
            f"  <string>{version}</string>",
            "  <key>CFBundleExecutable</key>",
            f"  <string>{product.exe}</string>",
            "  <key>CFBundleIconFile</key>",
            f"  <string>{product.name}</string>",
            "  <key>CFBundlePackageType</key>",
            "  <string>APPL</string>",
            "  <key>CFBundleInfoDictionaryVersion</key>",
            "  <string>6.0</string>",
            "  <key>LSMinimumSystemVersion</key>",
            "  <string>11.0</string>",
            "  <key>NSHighResolutionCapable</key>",
            "  <true/>",
            "  <key>LSApplicationCategoryType</key>",
            "  <string>public.app-category.entertainment</string>",
            "</dict>",
            "</plist>",
            "",
        ]),
        encoding="utf-8",
    )


def stage_macos_app(product: Product, version: str, build: Path, dist: Path) -> Path:
    app = dist / f"{product.name}.app"
    flat = dist / f"{product.name}-{version}"
    if app.exists():
        shutil.rmtree(app)
    if flat.exists():
        shutil.rmtree(flat)
    contents = app / "Contents"
    macos = contents / "MacOS"
    resources = contents / "Resources"
    fill_payload(product, build, resources, macos)
    icon = REPO / "packaging" / "macos" / f"{product.name}.icns"
    if not icon.is_file():
        raise SystemExit(f"missing {icon.relative_to(REPO)}")
    copy_file(icon, resources / f"{product.name}.icns")
    write_info_plist(contents / "Info.plist", product, version)
    (contents / "PkgInfo").write_text("APPL????", encoding="utf-8")
    log(f"staged {app.relative_to(REPO)}")
    return app


def is_system_macho(dep: str) -> bool:
    return dep.startswith("/usr/lib/") or dep.startswith("/System/")


def macho_id(path: Path) -> str | None:
    lines = capture(["otool", "-D", str(path)]).splitlines()
    if len(lines) >= 2 and lines[1].strip():
        return lines[1].strip()
    return None


def otool_deps(path: Path) -> list[str]:
    lines = capture(["otool", "-L", str(path)]).splitlines()[1:]
    deps = []
    for line in lines:
        text = line.strip()
        if not text:
            continue
        deps.append(text.split(" (compatibility version", 1)[0].strip())
    return deps


def macho_rpaths(path: Path) -> list[str]:
    lines = capture(["otool", "-l", str(path)]).splitlines()
    rpaths = []
    pending = False
    for line in lines:
        text = line.strip()
        if text.startswith("cmd LC_RPATH"):
            pending = True
            continue
        if pending and text.startswith("path "):
            rpaths.append(text.split(" ", 1)[1].rsplit(" (", 1)[0])
            pending = False
    return rpaths


def resolve_macho_dep(binary: Path, dep: str) -> Path:
    if dep.startswith("@executable_path/") or dep.startswith("@loader_path/"):
        rel = dep.split("/", 1)[1]
        candidate = binary.parent / rel
        if candidate.exists():
            return candidate.resolve()
    if dep.startswith("@rpath/"):
        name = dep.split("/", 1)[1]
        for rpath in macho_rpaths(binary):
            root = rpath
            for token, value in (
                ("@executable_path", str(binary.parent)),
                ("@loader_path", str(binary.parent)),
            ):
                root = root.replace(token, value)
            candidate = Path(root) / name
            if candidate.exists():
                return candidate.resolve()
    candidate = Path(dep)
    if candidate.is_file():
        return candidate.resolve()
    raise SystemExit(f"{binary.name} depends on {dep}, and that file was not found")


def sdl3_filename(blob: bytes) -> str | None:
    for name in (b"libSDL3.dylib", b"libSDL3.so.0", b"SDL3.dll"):
        if name in blob:
            return name.decode("ascii")
    return None


def find_sdl3(near: Path, filename: str) -> Path:
    names = [filename]
    if filename == "libSDL3.dylib":
        names.append("libSDL3.0.dylib")
    if filename == "libSDL3.so.0":
        names.extend(["libSDL3.so", "libSDL3.so.0"])
    candidates: list[Path] = []
    for name in names:
        candidates.append(near.parent / name)
        candidates.append(Path("/opt/homebrew/lib") / name)
        candidates.append(Path("/usr/local/lib") / name)
    brew = shutil.which("brew")
    if brew and filename.endswith(".dylib"):
        try:
            prefix = capture([brew, "--prefix", "sdl3"]).strip()
        except subprocess.CalledProcessError:
            prefix = ""
        if prefix:
            lib = Path(prefix) / "lib"
            candidates.append(lib / filename)
            candidates.append(lib / "libSDL3.0.dylib")
    if brew and filename.endswith(".so.0"):
        try:
            libdir = capture(["pkg-config", "--variable=libdir", "sdl3"]).strip()
        except (subprocess.CalledProcessError, FileNotFoundError):
            libdir = ""
        if libdir:
            candidates.append(Path(libdir) / filename)
    seen: set[Path] = set()
    for candidate in candidates:
        resolved = candidate.resolve() if candidate.exists() else None
        if resolved is None or resolved in seen:
            continue
        seen.add(resolved)
        if resolved.is_file():
            return resolved
    raise SystemExit(
        f"{near.name} loads {filename} at runtime, and that library was not found. "
        "Install SDL3 next to SDL2, or install the sdl3 package."
    )


def rewrite_macho_id(path: Path, install_name: str) -> None:
    make_writable(path)
    run(["install_name_tool", "-id", install_name, str(path)])


def rewrite_macho_dep(path: Path, old: str, new: str) -> None:
    make_writable(path)
    run(["install_name_tool", "-change", old, new, str(path)])


def bundle_macos(stage: Path, exes: list[Path]) -> list[Path]:
    """Copy non-system dylibs beside the executables and point at them."""
    machos = list(exes)
    index = 0
    bundled: list[Path] = []
    # Staged copy -> the library file it was copied from. SDL3 is not a
    # link dependency of sdl2-compat; it is dlopened from beside SDL2.
    copied_from: dict[Path, Path] = {}
    while index < len(machos):
        binary = machos[index]
        index += 1
        own = macho_id(binary)
        for dep in otool_deps(binary):
            if dep == own or is_system_macho(dep) or dep.startswith("@executable_path/"):
                continue
            src = resolve_macho_dep(binary, dep)
            target = stage / src.name
            install_name = f"@executable_path/{src.name}"
            if len(install_name) > len(dep):
                raise SystemExit(
                    f"cannot retarget {binary.name}: {install_name} is longer than {dep}. "
                    "Relink with a larger header pad."
                )
            if target not in machos:
                copy_file(src, target)
                copied_from[target] = src
                rewrite_macho_id(target, install_name)
                machos.append(target)
                bundled.append(target)
                log(f"  bundled {src.name}")
            rewrite_macho_dep(binary, dep, install_name)

        wanted = sdl3_filename(binary.read_bytes())
        if wanted is None:
            continue
        sdl3_target = stage / wanted
        if sdl3_target.exists():
            continue
        hint = copied_from.get(binary, binary)
        src = find_sdl3(hint, wanted)
        install_name = f"@executable_path/{wanted}"
        sdl3_id = macho_id(src) or str(src)
        if len(install_name) > len(sdl3_id):
            raise SystemExit(
                f"cannot retarget {wanted}: {install_name} is longer than its install name {sdl3_id}"
            )
        copy_file(src, sdl3_target)
        copied_from[sdl3_target] = src
        rewrite_macho_id(sdl3_target, install_name)
        machos.append(sdl3_target)
        bundled.append(sdl3_target)
        log(f"  bundled {wanted} (loaded by name at runtime)")

    for macho in machos:
        own = macho_id(macho)
        for dep in otool_deps(macho):
            if dep == own or is_system_macho(dep) or dep.startswith("@executable_path/"):
                continue
            raise SystemExit(f"{macho.name} still links an outside library: {dep}")
    return bundled


def sign_macho(path: Path, identity: str | None) -> None:
    make_writable(path)
    if identity is None:
        run(["codesign", "--force", "--sign", "-", str(path)])
        return
    run([
        "codesign", "--force", "--options", "runtime",
        "--timestamp", "--sign", identity, str(path),
    ])


def developer_id(explicit: str | None) -> str:
    if explicit:
        return explicit
    text = capture(["security", "find-identity", "-v", "-p", "codesigning"])
    found = []
    for line in text.splitlines():
        if "Developer ID Application:" not in line:
            continue
        start = line.find('"')
        end = line.rfind('"')
        if start != -1 and end > start:
            found.append(line[start + 1:end])
    if len(found) == 1:
        return found[0]
    if not found:
        raise SystemExit(
            "no Developer ID Application certificate in the keychain. "
            "Pass --identity or install the certificate."
        )
    joined = "\n".join(found)
    raise SystemExit(f"more than one Developer ID Application certificate:\n{joined}\nPass --identity.")


def notary_args() -> list[str] | None:
    profile = os.environ.get("APPLE_NOTARY_PROFILE")
    if profile:
        return ["--keychain-profile", profile]
    key = os.environ.get("APPLE_NOTARY_KEY")
    key_id = os.environ.get("APPLE_NOTARY_KEY_ID")
    issuer = os.environ.get("APPLE_NOTARY_ISSUER")
    if key and key_id and issuer:
        return ["--key", key, "--key-id", key_id, "--issuer", issuer]
    apple_id = os.environ.get("APPLE_ID")
    team = os.environ.get("APPLE_TEAM_ID")
    password = os.environ.get("APPLE_APP_PASSWORD")
    if apple_id and team and password:
        return ["--apple-id", apple_id, "--team-id", team, "--password", password]
    return None


def seal_app(app: Path, identity: str | None) -> None:
    if not (app / "Contents" / "Info.plist").is_file():
        return
    if identity is None:
        run(["codesign", "--force", "--sign", "-", str(app)])
    else:
        run([
            "codesign", "--force", "--options", "runtime",
            "--timestamp", "--sign", identity, str(app),
        ])
    run(["codesign", "--verify", "--strict", str(app)])


def sign_macos(stage: Path, exes: list[Path], bundled: list[Path], identity: str) -> None:
    run(["xattr", "-cr", str(stage)])
    # Libraries first. The executable signature is checked against them.
    for path in bundled + exes:
        sign_macho(path, identity)
        run(["codesign", "--verify", "--strict", str(path)])
    seal_app(stage, identity)


def make_dmg(app: Path, archive: Path, identity: str, volume_name: str) -> None:
    staging = app.parent / f".{app.stem}-dmg"
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir()
    run(["ditto", str(app), str(staging / app.name)])
    (staging / "Applications").symlink_to("/Applications")
    if archive.exists():
        archive.unlink()
    # hdiutil create -volname -format is deprecated on current macOS.
    run([
        "diskutil", "image", "create", "from",
        "--format", "UDZO",
        "--volumeName", volume_name,
        str(staging),
        str(archive),
    ])
    shutil.rmtree(staging)
    run(["codesign", "--force", "--timestamp", "--sign", identity, str(archive)])
    creds = notary_args()
    if creds is None:
        log("signed the DMG. Notarization was skipped.")
        log("Set APPLE_NOTARY_PROFILE, or APPLE_NOTARY_KEY / APPLE_NOTARY_KEY_ID / APPLE_NOTARY_ISSUER,")
        log("or APPLE_ID / APPLE_TEAM_ID / APPLE_APP_PASSWORD, then run again with --sign.")
        return
    run(["xcrun", "notarytool", "submit", str(archive), *creds, "--wait"])
    run(["xcrun", "stapler", "staple", str(archive)])
    run(["xcrun", "stapler", "validate", str(archive)])
    log(f"notarized {archive.name}")


def linux_should_bundle(soname: str) -> bool:
    if soname.startswith("libSDL"):
        return True
    if soname in LINUX_SYSTEM_SONAMES or soname.startswith("ld-linux"):
        return False
    if soname.startswith("libgcc") or soname.startswith("libstdc++"):
        return False
    return False


def ldd_deps(path: Path) -> list[tuple[str, Path]]:
    try:
        text = capture(["ldd", str(path)])
    except subprocess.CalledProcessError as exc:
        raise SystemExit(f"ldd failed for {path}") from exc
    found = []
    for line in text.splitlines():
        if "=>" not in line:
            continue
        soname, rest = line.split("=>", 1)
        soname = soname.strip()
        token = rest.strip().split()[0]
        if token == "not":
            raise SystemExit(f"{path.name} is missing {soname}")
        found.append((soname, Path(token)))
    return found


def bundle_linux(stage: Path, exes: list[Path]) -> None:
    if shutil.which("patchelf") is None:
        raise SystemExit("patchelf is required to package on Linux. Install patchelf and run again.")
    pending = list(exes)
    seen: set[str] = set()
    index = 0
    while index < len(pending):
        binary = pending[index]
        index += 1
        for soname, src in ldd_deps(binary):
            if not linux_should_bundle(soname) or soname in seen:
                continue
            target = stage / soname
            copy_file(src, target)
            make_writable(target)
            seen.add(soname)
            pending.append(target)
            log(f"  bundled {soname}")
        wanted = sdl3_filename(binary.read_bytes())
        if wanted and wanted not in seen:
            src = find_sdl3(binary, wanted)
            target = stage / wanted
            copy_file(src, target)
            make_writable(target)
            seen.add(wanted)
            pending.append(target)
            log(f"  bundled {wanted} (loaded by name at runtime)")
    for binary in pending:
        run(["patchelf", "--set-rpath", "$ORIGIN", str(binary)])


def is_windows_system_path(path: Path) -> bool:
    lowered = str(path).lower().replace("/", "\\")
    return "\\windows\\system32\\" in lowered or "\\windows\\syswow64\\" in lowered


def is_vc_runtime(name: str) -> bool:
    lowered = name.lower()
    return (
        lowered.startswith("vcruntime")
        or lowered.startswith("msvcp140")
        or lowered in {"concrt140.dll", "vcomp140.dll"}
    )


def find_vc_redist(name: str) -> Path | None:
    program_files_x86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(program_files_x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        return None
    arch = {"AMD64": "x64", "ARM64": "arm64", "x86": "x86"}.get(platform.machine(), "x64")
    pattern = f"VC\\Redist\\MSVC\\*\\{arch}\\Microsoft.VC*.CRT\\{name}"
    try:
        text = capture([str(vswhere), "-latest", "-products", "*", "-find", pattern])
    except subprocess.CalledProcessError:
        return None
    lines = sorted(line.strip() for line in text.splitlines() if line.strip())
    if not lines:
        return None
    return Path(lines[-1])


def find_windows_dll(name: str, build_exe_dir: Path) -> Path:
    local = build_exe_dir / name
    if local.is_file():
        return local
    if is_vc_runtime(name):
        redist = find_vc_redist(name)
        if redist is not None and redist.is_file():
            return redist
    found = shutil.which(name)
    if found:
        path = Path(found)
        if not is_windows_system_path(path) or is_vc_runtime(name):
            return path
    raise SystemExit(
        f"could not find {name} beside {build_exe_dir} or in the VC redist. "
        "Build from a Visual Studio developer shell."
    )


def dumpbin_deps(path: Path) -> list[str]:
    if shutil.which("dumpbin") is None:
        raise SystemExit("dumpbin is not on PATH. Run the package script from a Visual Studio developer shell.")
    text = capture(["dumpbin", "/nologo", "/dependents", str(path)])
    deps = []
    started = False
    for line in text.splitlines():
        stripped = line.strip()
        if "dependencies:" in stripped.lower():
            started = True
            continue
        if not started:
            continue
        if not stripped:
            if deps:
                break
            continue
        if stripped.lower().endswith(".dll"):
            deps.append(stripped)
    return deps


def bundle_windows(stage: Path, exes: list[Path], build_exe_dir: Path) -> None:
    pending = list(exes)
    seen: set[str] = set()
    index = 0
    while index < len(pending):
        binary = pending[index]
        index += 1
        for dep in dumpbin_deps(binary):
            lowered = dep.lower()
            if lowered in seen or lowered in WINDOWS_SYSTEM_DLLS or lowered.startswith("api-ms-win-") or lowered.startswith("ext-ms-"):
                continue
            if is_windows_system_path(Path(dep)) and not is_vc_runtime(dep):
                continue
            src = find_windows_dll(dep, build_exe_dir)
            if is_windows_system_path(src) and not is_vc_runtime(dep):
                seen.add(lowered)
                continue
            target = stage / src.name
            if not target.exists():
                copy_file(src, target)
                log(f"  bundled {src.name}")
            seen.add(lowered)
            pending.append(target)
        wanted = sdl3_filename(binary.read_bytes())
        if wanted and wanted.lower() not in seen:
            src = find_windows_dll(wanted, build_exe_dir)
            copy_file(src, stage / wanted)
            seen.add(wanted.lower())
            pending.append(stage / wanted)
            log(f"  bundled {wanted} (loaded by name at runtime)")


def zip_tree(stage: Path, archive: Path) -> None:
    if archive.exists():
        archive.unlink()
    root = stage.name
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as zipped:
        for path in sorted(p for p in stage.rglob("*") if p.is_file()):
            info = zipfile.ZipInfo(f"{root}/{path.relative_to(stage).as_posix()}")
            info.date_time = time.localtime(path.stat().st_mtime)[:6]
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (path.stat().st_mode & 0xFFFF) << 16
            zipped.writestr(info, path.read_bytes())
    log(f"wrote {archive.relative_to(REPO)}")


def tar_tree(stage: Path, archive: Path) -> None:
    if archive.exists():
        archive.unlink()
    with tarfile.open(archive, "w:gz") as tar:
        tar.add(stage, arcname=stage.name)
    log(f"wrote {archive.relative_to(REPO)}")


def bundle_runtime(stage: Path, build: Path, sign: bool, identity: str | None) -> None:
    system = platform.system()
    bin_dir = stage / "Contents" / "MacOS"
    if not bin_dir.is_dir():
        bin_dir = stage
    exe_paths = []
    for name in ("a2m", "c64m", "am65"):
        filename = name + (".exe" if system == "Windows" else "")
        candidate = bin_dir / filename
        if candidate.is_file():
            exe_paths.append(candidate)
    if system == "Darwin":
        # exes were copied from the build tree and still point at Homebrew.
        bundled = bundle_macos(bin_dir, exe_paths)
        if sign:
            assert identity is not None
            sign_macos(stage, exe_paths, bundled, identity)
        else:
            run(["xattr", "-cr", str(stage)])
            for path in bundled + exe_paths:
                sign_macho(path, None)
            seal_app(stage, None)
        return
    if system == "Linux":
        bundle_linux(stage, exe_paths)
        return
    if system == "Windows":
        build_dir = find_exe(build, "a2m").parent
        bundle_windows(stage, exe_paths, build_dir)
        return
    raise SystemExit(f"unsupported operating system {system}")


def archive_product(
    stage: Path,
    product_name: str,
    version: str,
    sign: bool,
    identity: str | None,
) -> None:
    tag = os_tag()
    system = platform.system()
    stem = f"{product_name}-{version}-{tag}"
    if system == "Darwin" and sign:
        assert identity is not None
        make_dmg(stage, stage.parent / f"{stem}.dmg", identity, f"{product_name} {version}")
        return
    if system == "Linux":
        tar_tree(stage, stage.parent / f"{stem}.tar.gz")
        return
    zip_tree(stage, stage.parent / f"{stem}.zip")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Package a2m and c64m for GitHub.")
    parser.add_argument("--build-dir", type=Path, default=REPO / "build-package")
    parser.add_argument("--dist", type=Path, default=REPO / "dist")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument(
        "--sign",
        action="store_true",
        help="macOS: sign with Developer ID, write a DMG, and notarize when credentials are set",
    )
    parser.add_argument("--identity", help="macOS codesign identity")
    parser.add_argument(
        "--cmake-args",
        action="append",
        default=[],
        help="extra argument passed to cmake configure. Repeat for each argument",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.sign and platform.system() != "Darwin":
        raise SystemExit("--sign is the macOS Developer ID / DMG / notarize path")
    build = args.build_dir if args.build_dir.is_absolute() else (Path.cwd() / args.build_dir).resolve()
    dist = args.dist if args.dist.is_absolute() else (Path.cwd() / args.dist).resolve()
    dist.mkdir(parents=True, exist_ok=True)

    identity = None
    if args.sign:
        identity = developer_id(args.identity)
        log(f"signing with {identity}")

    if not args.skip_build:
        configure_and_build(build, args.cmake_args)

    for product in PRODUCTS:
        version = read_version(product)
        if platform.system() == "Darwin":
            stage = stage_macos_app(product, version, build, dist)
        else:
            stage = stage_product(product, version, build, dist)
        bundle_runtime(stage, build, args.sign, identity)
        archive_product(stage, product.name, version, args.sign, identity)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except subprocess.CalledProcessError as exc:
        command = exc.cmd if isinstance(exc.cmd, str) else " ".join(exc.cmd)
        log(f"command failed ({exc.returncode}): {command}")
        sys.exit(exc.returncode or 1)
