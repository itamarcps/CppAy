# Building and installing

## Linux desktop

Use a 64-bit C++20 compiler, CMake 3.24+, Ninja and Qt 6.8+ with Quick,
QuickControls2, Multimedia, Concurrent, Network and Widgets. Tests require
Python 3 with `requirements-dev.txt` installed. Qt 6.11.2 / GCC 16.2 are locally
verified; CI installs Qt 6.11.2 with aqtinstall on Ubuntu 24.04.

```sh
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release --no-tests=error
./build/C++Ay
```

A non-system Qt installation needs `-DCMAKE_PREFIX_PATH=/path/to/Qt/gcc_64`.
CMake release optimization follows the compiler's Release configuration
(`-O3 -DNDEBUG` for the verified GCC/MinGW builds).

The core/CLI can build without Qt:

```sh
cmake -S . -B build-cli -G Ninja -DCMAKE_BUILD_TYPE=Release -DAYPLAYER_GUI=OFF
cmake --build build-cli --parallel
ctest --test-dir build-cli --output-on-failure --no-tests=error
```

`AYPLAYER_SANITIZERS=ON` enables ASan/UBSan with GCC/Clang.
`BUILD_TESTING=OFF` omits test executables and the Python requirement.
`AYPLAYER_GOLDEN_TESTS=ON` adds optional private sample comparisons.
`AYPLAYER_MEMORY_BASELINE=ON` is diagnostic only; do not enable it for releases.
Legacy `AYPLAYER_*` configuration and internal QML/storage names remain for
compatibility; the installed executable and displayed brand are C++Ay.

Install into a reviewable local tree:

```sh
cmake --install build --prefix "$PWD/build/install"
```

The desktop install includes the player, CLI, desktop entry, icon, MIME definition,
license and attribution. System Qt libraries/plugins are required. A core-only
install includes the CLI and notices. Desktop file associations use the normal
registration facilities of the destination desktop environment.

## Windows cross-compilation

```sh
./tools/build_windows.sh
```

This produces `build-windows/C++Ay-windows-x64.zip`, using
`cmake/toolchains/mingw64.cmake` and SHA-256-pinned downloads listed in
`packaging/windows/dependencies.lock.json`.

Host requirements: Linux x86-64, CMake/Ninja, Python 3.11+, curl, tar with
Zstandard support, 7-Zip and native Linux Qt **6.11.2** tools/modules. The
extracted cross-compiler also needs compatible glibc, GMP, MPFR, MPC, ISL, zlib
and Zstandard libraries. The bootstrap is locally verified on CachyOS/Arch.
The manual GitHub workflow supplies these dependencies on Ubuntu 24.04; it has
not been executed remotely before publication.

The compiler is MinGW GCC 13.1, MinGW-w64 11, POSIX/SEH/MSVCRT, matching the
Qt 6.11.2 SDK. A system UCRT compiler is not interchangeable with that SDK.
Downloads remain in `.deps/`; no system SDK is installed by the scripts.

If needed, set `QT_HOST_PATH` to your Linux Qt installation. Custom SDK prefixes
can be supplied using `AYPLAYER_MINGW_ROOT`, `AYPLAYER_WINDOWS_QT` and the
packager's `--mingw`, `--qt`, `--qt-host` options. To rerun after setup:

```sh
cmake --preset windows-mingw64
cmake --build --preset windows-mingw64 --parallel
python3 tools/package_windows.py
```

The packager scans QML imports, resolves DLLs recursively, validates all PE
binaries as x86-64, includes dependency licenses/SBOMs and creates checksums.
No music or original source archives are bundled. Qt dependencies are dynamically
linked and the runtime DLLs remain replaceable.

On a graphical Linux desktop with Wine and NumPy installed:

```sh
./tools/build_windows.sh --verify
```

This runs independent fixtures, optional private golden checks, Unicode paths,
overwrite refusal, playback/seek/export, rapid transitions and three/six-channel
layouts. An isolated Wine prefix lives under `.deps/`. Physical Windows device
validation remains outstanding. Qt 6.11 targets Windows 11 x64.


## Prepared Linux binary archive

```sh
python3 tools/package_linux.py --build-dir build --output build/packages
```

Use a fresh package output directory. This packages the installed executable,
CLI, desktop/icon/MIME resources and project notices. Its Qt QML-import scanner
checks system modules/plugins; ELF dependency and required symbol-version records
are retained in `PACKAGE.json`. Application QML/branding is embedded. Safe
`$ORIGIN` relative runtime paths are retained; absolute development RPATHs are
rejected. No host/Qt libraries are copied.

This is the declared **system-runtime model**, not a self-contained Qt bundle.
The prepared CachyOS package requires compatible CachyOS/Arch Qt 6.11.2,
GCC runtime/glibc, QML styles, platform plugins and audio backend packages.
Font/theme/translation resources come from those system packages. Building on
an older distribution yields that distribution's corresponding dependency model;
the current binary is not advertised as universally portable.

The exact archive was extracted in `/tmp`, launched outside source/build folders
with Qt/QML/LD development overrides removed and fresh XDG settings, exercised
on KDE Wayland with real QtAudio output, and its GUI WAV re-compared against the
original direct export. This verifies the declared host-runtime scenario, not a
fresh machine without dependencies. See [candidate results](RELEASE_NOTES.md).

Qt provides [QML deployment helpers](https://doc.qt.io/qt-6.8/qt-generate-deploy-qml-app-script.html)
for runtime-bundling models. This archive deliberately depends on system Qt;
Windows uses the existing cross-host import/DLL deployment tool with complete
runtime notices. A suppressed unsupported-platform deployment call is not used
to claim a complete bundle.
