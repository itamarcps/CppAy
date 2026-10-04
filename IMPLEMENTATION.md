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
