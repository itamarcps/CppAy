# Memory investigation

Before optimization, a process on the Linux development machine was measured read-only at approximately 499 MiB RSS and 359 MiB PSS. Its mappings included CUDA, NVIDIA graphics/compiler libraries and LLVM, plus large private allocations. RSS includes resident shared library pages; PSS apportions those pages between processes.

Two independent startup comparisons isolated GPU overhead. Qt Quick's GPU renderer accounted for substantial additional memory. Qt Multimedia's FFmpeg backend also probed video hardware devices while initializing media integration, although this player only sends synthesized PCM to QAudioSink. Selecting the software scene graph and an empty FFmpeg hardware-device list removed the observed GPU libraries from the optimized process. Both defaults are selected before QApplication, and explicit Qt environment overrides remain available.

The software scene graph is appropriate for the existing 2D interface, including its Canvas scopes, gradients and beveled controls. Audio synthesis, independent voice taps, seeking, WAV rendering and the periodic session checkpoint are unchanged.

## Measurements

A controlled release-build comparison used identical source and workload, with a diagnostic compile definition restoring only the original Qt defaults in the baseline. Both processes loaded 362 tracks, played actual audio output, changed tracks every two seconds, displayed three/six voices and ran for one minute.

| Metric | Original defaults | Optimized defaults |
|---|---:|---:|
| Steady resident RAM (RSS) | 261.7 MiB | 129.1 MiB |
| Steady proportional RAM (PSS) | 120.2 MiB | 48.8 MiB |
| Peak resident RAM | 264.5 MiB | 130.4 MiB |
| CPU, percent of one logical core | 25.8% | 19.6% |
| Audio underruns | 0 | 0 |

The optimized process mapped none of the CUDA/NVIDIA/LLVM libraries observed in the baseline. The raw playback, single-song and original-process reports are retained locally under `evidence/` and excluded from Git because they contain machine-specific data. The measurement tool can produce fresh reports on another machine. Figures depend on system drivers and which library pages are shared. They are Linux process measurements, not a claim of identical memory usage on physical Windows hardware.

The existing playback queue remains bounded at approximately 354 ms, history at two seconds, and seek checkpoints at 64 entries. Playlist imports read metadata without rendering PCM. Full-song WAV export intentionally holds the rendered PCM temporarily and can use more memory than normal playback.

## Reproduce on Linux

Build the production player normally. Build a separate diagnostic baseline:

```sh
cmake -S . -B build-memory-baseline -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DAYPLAYER_MEMORY_BASELINE=ON
cmake --build build-memory-baseline -j 8
python tools/measure_memory.py --seconds 60 --import-folder /path/to/music
python tools/measure_memory.py --optimized-only --single-track --seconds 120 --import-folder /path/to/music --report evidence/memory-single-track.json
```

The measurement tool uses isolated configuration/data directories and its own child processes. Audio is muted in the diagnostic workload while the real audio sink, synthesis and visualizers continue to run. It samples Linux `/proc` RSS/PSS and process CPU counters, imports metadata, and records playback/voice/underrun checks. The diagnostic baseline is excluded from normal builds and packages.

A repeated baseline run hit the NVIDIA `NV_ERR_STATE_IN_USE` allocation failure already observed on this host. The optimized two-minute playback workload completed successfully; its repeated run records complete memory samples independently of that failing GPU baseline.

Qt references: [scene graph adaptations](https://doc.qt.io/qt-6/qtquick-visualcanvas-adaptations.html), [software adaptation](https://doc.qt.io/qt-6/qtquick-visualcanvas-adaptations-software.html), and [FFmpeg hardware-device configuration](https://doc.qt.io/QT-6/advanced-ffmpeg-configuration.html).
