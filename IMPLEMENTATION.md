# Implementation and current coverage

C++Ay shares one native engine between incremental playback, CLI rendering and
WAV export. This is a focused AY/YM player; complete AY_Emul parity remains
unfinished.

| Area | Implemented | Remaining |
| --- | --- | --- |
| PT3 | Native decoding, note/volume tables, tested effects and PT3.7 TurboSound | Broader real-world corpus validation |
| Logs | PSG versions 0–10, timing override precedence, YM3/YM3b | Other PSG variants and YM4–YM6 |
| Chip/output | AY/YM amplitude models, source mixer/filter/interpolation, qualified stereo PCM | Other chip/clock modes and full upstream timing matrix |
| Playback | Background streaming, pause/stop/seek, bounded checkpoints, three/six real voice taps | Subsong selection, device chooser, format loop points |
| Playlist | Embedded titles, duration, bulk native chooser, recursive folders, reorder, M3U, remove/clear | AYL, shuffle and richer folder inclusion modes |
| Export | Fresh-from-start WAV, cancellation, no overwrite, batch WAV, PSG/event CLI output | Broader conversion tools and optional adapters |
| UI | Compact retro desktop layout, grouped mixer, clipped independently triggered scopes | Broader DPI/accessibility/platform checks |
| Persistence | Atomic session checkpoints every ten seconds and on clean exit; crash/resume checks | Tray and shutdown integration beyond normal Qt lifecycle |
| Platforms | Linux x86-64 and MinGW Windows x64 builds; Wine qualification | Physical Windows hardware validation |

Playback prepares a 40 ms block and queues about 354 ms of PCM. Observer history
is bounded to two seconds, with at most 64 seek checkpoints. Seeking restores
chip, filter, interpolation and decoder state; distant uncached seeks run in a
worker. Playlist imports read metadata without synthesizing PCM.

Scopes show real pre-pan voice samples. Each independently triggers on a rising
midpoint crossing using up to 100 ms of past samples, including low tones.
The displayed window is 20 ms; Canvas clipping and bounded vertical coordinates
keep it inside its panel. Noise and changes in pitch or envelope still change
its shape. Scope analysis does not alter stereo audio.

The independently qualified defaults are YM2149F, 1,773,400 Hz, 50 Hz interrupts,
ABC gains 255/13, 170/170, 13/255, preamp 127 and the source's 49-tap FIR.
A privately supplied direct AY_Emul WAV export matched all 2,457,879 stereo
frames at 48 kHz/16-bit without trimming, alignment or gain correction. This
qualifies that fixture/profile, not all upstream functionality. The original
export executable version and architecture are unknown. The music pair stays
local and is tested only when explicitly enabled.

Public independent checks validate twelve synthetic source-oracle PCM/event
fixtures with immutable hashes. Integration checks exercise canonical streaming,
seek, independent/silent channels, scope triggering, bulk metadata import,
controller export and real ten-second crash checkpoints. See
[testing](docs/TESTING.md) and [reference provenance](reference/README.md).

## Release preparation checkpoint — 0.1.0-rc.1 (2026-10-04)

Baseline: `main`, `c65090aecb270530490bca589800f0a0223c2845`, no remote.
The working tree already contained the custom-window implementation, its tests,
CI/docs edits and screenshot. They are preserved in this candidate. C++20,
CMake 3.24+, Qt 6.8+ minimum remain; local Release uses Qt 6.11.2/GCC 16.2.1,
`-O3 -DNDEBUG`. Outputs are `C++Ay` and `aytool`; Windows equivalents use `.exe`.

| Gap established from this checkout | Fix / acceptance check |
| --- | --- |
| Comparator accepted any +8 RIFF size and lacked full diagnostics | Restrict exception to immutable original WAV; schema 2, per-channel/windows/location/peak/DC/clipping reports; independent negative tests |
| Residual could overflow 16-bit and abort | Unscaled IEEE float32 residual, full-range regression |
| Export collision did not emit completion/error signal | Emit actionable failure synchronously; assert original destination/cursor preserved |
| Smoke test relied on precreated report directory; some report fields claimed success unconditionally | Create/check directory; report check outcomes and optional import coverage honestly |
| Last-track EOF/repeat lacked focused actual-device regression | Device transport test; explicitly skipped with code 77 if no device |
| No single release evidence command, plots or tested A/B report | Shared `aytool` + strict comparator + `release_fidelity.py`; missing required pair fails; independent repeatability/observer qualification and profiles |
| No Linux release archive/runtime record | System-Qt binary archive, QML/plugin scan, ELF dependencies/symbol versions, clean extraction/device/export verification |
| README lacked measurable evidence/current screenshot | User-supplied real screenshot, generated numerical table, public plot/audio example, linked complete criteria |
| No consistent candidate version/provenance | 0.1.0-rc.1 CLI/About/PE resource/package version; source snapshot, reports and checksums |

Full original implementation scope remains **unfinished**. A qualified focused
candidate cannot satisfy the original complete-AY_Emul application requirement:
other native tracker families, CPU-backed AY/EMUL and AYM, SNDH/68000/MFP/DMA,
YM4–6/digidrums/VTX/depacking/container families, structural music finder,
AYL/PLS/CUE and per-item/subsong overrides, subsong UI, device chooser, tray and
broader conversions are not implemented. These are implementation work, not
external blockers and not silently removed from the requirements. Existing
exclusions remain ZX50, PSG2/BK and standalone AS0 conversion outside baseline.

The verified candidate covers PT3/PT3.7 TurboSound, supported PSG, YM3/YM3b and
existing playback/playlist/mixer/export/session UI. Required private music bytes
are found and preserved. Their redistribution permission is absent; private
verification is allowed, public distribution of that music is not assumed.
Original export binary version/architecture are unknown; this is disclosed,
while the source-default profile is qualified against its full direct export.

The production path is loader (`pt3.cpp`/`logs.cpp`) → ordered register events
and source integer interrupt timing → AY/YM chip state → source gain tables,
FIR/averager and interpolation → signed stereo 16-bit PCM (`engine.cpp`).
`StreamRenderer` supplies both live `PlaybackSession` and `renderFile` used by
controller WAV export and `aytool`; QtAudio applies logarithmic playback volume
at the device sink only. No separate fidelity synthesizer exists. Offline tests
do not certify the sound server, device, DAC or browser.

Final executed results, artifact identities and outstanding gates are retained
in [release notes](docs/RELEASE_NOTES.md) and [fidelity](docs/FIDELITY.md).
