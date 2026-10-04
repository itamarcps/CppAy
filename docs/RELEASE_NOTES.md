# C++Ay 0.1.0

A native AY/YM chiptune player with a retro desktop interface.

## Features

- PT3, PT3.7 TurboSound, PSG 0–10 and YM3/YM3b playback.
- Native file selection, recursive folder import, drag and drop, M3U playlists
  and embedded track titles.
- Streaming playback, seeking, repeat, logarithmic volume and three/six
  independent channel oscilloscopes.
- Compact chip/stereo/output mixer and WAV/batch export from the beginning.
- Session recovery with automatic checkpoints every ten seconds.
- Custom window controls and a slate, silver and amber theme.

## Downloads

- [Windows x64 ZIP](https://github.com/itamarcps/CppAy/releases/download/v0.1.0/C%2B%2BAy-0.1.0-windows-x64.zip): extract everything and run `C++Ay.exe`.
- [Linux x86-64 archive](https://github.com/itamarcps/CppAy/releases/download/v0.1.0/C%2B%2BAy-0.1.0-linux-x86_64.tar.gz): extract and run `bin/C++Ay`. Requires the compatible CachyOS/Arch Qt 6.11.2, GCC and glibc runtime listed in `INSTALL.txt` and `PACKAGE.json`.
- [Source archive](https://github.com/itamarcps/CppAy/releases/download/v0.1.0/C%2B%2BAy-0.1.0-source.tar.gz): build with C++20, CMake 3.24+ and Qt 6.8+.
- [Audio evidence](https://github.com/itamarcps/CppAy/releases/download/v0.1.0/C%2B%2BAy-0.1.0-public-evidence.tar.gz): independent synthetic references, C++Ay audio, measurements and an offline listening report.
- [SHA-256 checksums](https://github.com/itamarcps/CppAy/releases/download/v0.1.0/SHA256SUMS).

Linux uses system Qt; the archive is not a universal portable bundle. Windows
includes Qt/MinGW runtimes and their notices. Windows binaries were tested under
Wine; physical Windows hardware remains unverified. No macOS build is provided.

## Verification

Native Release tests passed 12/12. The qualification corpus passed 27/27
**BIT_EXACT_PCM**, including the complete 2,457,879-frame direct AY_Emul export.
The redistributable subset passed 12/12 with exact ordered register events.
Offline PCM equivalence does not certify a sound server or physical audio device.

[Build and sanitizer CI](https://github.com/itamarcps/CppAy/actions/workflows/ci.yml) ·
[Measurements and provenance](https://github.com/itamarcps/CppAy/blob/main/docs/FIDELITY.md) ·
[Build instructions](https://github.com/itamarcps/CppAy/blob/main/docs/BUILDING.md).

## Limitations

Support is limited to the formats above. CPU-backed AY/AYM, SNDH, other tracker
families, YM4–6/VTX, subsong selection, structural music search, AYL/PLS/CUE,
audio-device selection and tray integration are not implemented.

[Format and feature coverage](https://github.com/itamarcps/CppAy/blob/main/IMPLEMENTATION.md) ·
[License and component notices](https://github.com/itamarcps/CppAy/blob/main/THIRD_PARTY.md).
