# C++Ay 0.1.0-rc.1 — local prerelease candidate

**Focused candidate prepared and verified; full original AY_Emul scope remains
unfinished.** No repository, tag, assets or release have been published.

C++Ay is a native C++20/Qt Quick chiptune player for Linux, preserving a
slate/silver/amber late-2000s interface. It streams PT3/PT3.7 TurboSound,
PSG 0–10 and YM3/YM3b, with compact native-import playlists, three/six voice
scopes, mixer settings, seeking, logarithmic volume, WAV/batch export and
atomic ten-second crash-resume checkpoints.

## Changes in this pass

- Retained the previously implemented custom header, branding and window controls.
- Fixed WAV-collision error notification; existing audio/cursor remain intact.
- Fixed evidence-directory creation and misleading smoke-result fields.
- Added actual-device EOF/pause/resume/stop/repeat regression.
- Strengthened comparator bounds/metadata, quiet/right-channel/final-tail checks,
  full diagnostics, threshold/event negatives and float residual range.
- Added one-command evidence, real numerical plots, tested offline A/B playback
  and a range-aware dependency-free listening server.
- Prepared Linux system-runtime packaging and consistent version/build provenance;
  updated lean CI without automatic publication.
- Rewrote README with the supplied actual screenshot, measured results, public
  listening sample, tested commands, honest limitations and credits.

## Verification executed locally

| Gate | Result |
| --- | --- |
| Isolated native Release configure/build | PASS; GCC 16.2.1, Qt 6.11.2, C++20, `-O3 -DNDEBUG` |
| Native CTest, private pair enabled | **12/12 passed**, no skips; includes sixteen comparator cases |
| Core ASan/UBSan | **5/5 passed**, no sanitizer findings |
| Public source snapshot, fresh `/tmp` build | **9/9 passed**, no skips; private fixtures absent |
| Public independent fidelity | **12/12 BIT_EXACT_PCM**, ordered events exact |
| Full packaged-renderer qualification | **27/27 BIT_EXACT_PCM**; reports cover each channel/full interval/windows |
| Pascal repeatability / instrumentation | Three renders identical; original direct export qualification bit-exact |
| Required pair missing in fresh public source | Correct nonzero exit: **BLOCKED_MISSING_REFERENCE** |
| Extracted Linux package, fresh XDG settings, development overrides removed | PASS on declared CachyOS/KDE Wayland runtime |
| Packaged GUI WAV vs original reference | **BIT_EXACT_PCM**, 2,457,879 stereo frames, zero unequal samples |
| Real QtAudio playback / rapid track changes | PASS; recorded zero underruns; cursor/mixer/List tools/import while playing verified |
| Three/six-channel layout at 125% scaling | Eight size/channel cases PASS; compact controls/resources checked |
| Custom header | Maximize/restore, double-click, minimize, close and eight resize handles PASS |
| Browser listening report | PASS: local paths/audio, A/B cursor preservation, exclusive playback and pause/reverse switch |
| MinGW Windows x64 / Wine | PASS: twelve independent cases, full private PCM, Unicode/no-overwrite, GUI export, audio/streaming, layouts/header |
| Physical Windows / macOS | **NOT EXECUTED** |
| GitHub Actions | Workflow validated with actionlint; **NOT EXECUTED remotely** (no remote) |

These results certify rendered PCM in the stated scope. The device checks establish
application/backend playback behavior, not numerical equivalence through a sound
server, DAC or physical device. Wayland system drag/resize requires real pointer
input; automated header checks establish controls/handle layout, not every compositor's
pointer-driven geometry behavior. No fresh-machine portability claim is made.

## Fidelity identity and results

The production implementation/verification source revision is
`4dc0dbd` (full hash in the generated manifest). Subsequent documentation/evidence
commits do not alter its application/core/tool inputs. The installed renderer
SHA-256 is `a3c3726c1acef1378343380969e76239cb564364c7cc22e28e4c689092e53eb9`.
The exact compiler, Qt version, flags, CMake-cache hash, fixture identities,
profile and commands are retained in the evidence manifest.

Original supplied Flexo02 direct AY_Emul export: 48 kHz, signed 16-bit, L/R,
2,457,879 frames (51.2058125 seconds). **BIT_EXACT_PCM**, zero unequal samples,
maximum error zero LSB, each channel whole-track and worst-window relative RMS
zero; residual SNR infinite (explicit null/status in JSON). No trim, fitted
offset/gain or missing tail. Original export executable version/architecture
are unknown; fixed source-default profile is inferred and golden-qualified.
Independent Pascal routines originate in AY_Emul 3.0 beta, not the unknown
original export binary. The original WAV has no original event trace; the
separate qualified source-oracle case has exact ordered events.

[Generated full table](fidelity/summary.md) · [manifest and contract](FIDELITY.md) ·
[public listening example](fidelity/listening/index.html).

Local evidence:

- `build-release-candidate/candidate-evidence/` — complete **private** 27-case
  evidence, per-case WAV/JSON/plots/residuals, qualification, commands and checksums.
- `build-release-candidate/candidate-evidence.tar.gz` — **private; do not share**.
- `build-release-candidate/public-release-evidence/` — public twelve-case offline report.
- `build-release-candidate/runtime-evidence/` — installed GUI/device/layout/browser/Wine reports.

Reproduce the exact installed-renderer gate:

```sh
python3 tools/release_fidelity.py --renderer build-release-candidate/release/C++Ay-0.1.0-rc.1-linux-x86_64/bin/aytool --build-dir build-release-candidate --output build-release-candidate/reproduced-evidence --private --oracle reference/oracle/oracle
```

## Packages and installation

All public candidate artifacts, exact sizes and SHA-256 identities are in
`build-release-candidate/release/release-manifest.json`; verify with:

```sh
cd build-release-candidate/release
sha256sum -c SHA256SUMS
```

- `C++Ay-0.1.0-rc.1-linux-x86_64.tar.gz`: extract and run `bin/C++Ay`.
  Requires compatible installed CachyOS/Arch Qt 6.11.2/GCC/glibc, QML/style,
  Wayland/XCB and audio runtime packages. Read `INSTALL.txt`/`PACKAGE.json`.
  Libraries/plugins/fonts/translations are supplied by the declared system runtime;
  this archive is not universally portable. Embedded QML/branding is included.
- `C++Ay-0.1.0-rc.1-windows-x64.zip`: extract everything and run `C++Ay.exe`.
  Bundled matching Qt/MinGW DLLs/QML/plugins and original runtime notices/SBOMs.
  Wine qualified; physical Windows 11 x64 remains untested.
- `C++Ay-0.1.0-rc.1-source.tar.gz`: public source, synthetic tests and small docs
  evidence; excludes private music, builds/SDKs and reference executables.
- `C++Ay-0.1.0-rc.1-public-evidence.tar.gz`: offline public listening/measurement bundle,
  suitable for sharing; no private reference audio.

Original project/branding is MIT; AY_Emul-derived routines retain Sergey Bulba's
notice. Qt/MinGW and other distributed components retain their own terms.
[Notices](../THIRD_PARTY.md).

## Remaining required work and publication limits

Full original scope is not finished: CPU-backed AY/AYM, SNDH/68000/MFP/DMA,
other tracker families, YM4–6/digidrums/VTX/depacking/containers, structural
music finder, subsong UI, AYL/PLS/CUE, item overrides, device chooser, tray and
broader conversion paths remain implementation work in
[IMPLEMENTATION.md](../IMPLEMENTATION.md). Selected PT3 results cannot certify
those paths. Existing exclusions remain explicit. Broader real-song/DPI/a11y
and physical Windows validation remain limited.

The original real music has no established redistribution permission: its
private evidence cannot be a public release asset. Numerical summaries and
original synthetic examples can be shared. The missing publication destination
is a chosen GitHub repository/remote. [Publication commands](RELEASING.md) are
prepared; no push/upload/tag/public-release action was executed.
