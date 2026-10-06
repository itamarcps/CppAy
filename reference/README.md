# Independent source reference

`oracle/` retains minimal diagnostic Pascal adapters built from Sergey Bulba's
AY_Emul routines, the observer patch and source hashes in `provenance.json`.
The original source collections, archives, toolchains and compiled adapters
are local development material and are excluded from Git.

The adapters are not player runtime dependencies. Their outputs are the
immutable synthetic PCM/event fixtures in `tests/fixtures`; ordinary tests
validate those files and never rebuild expected results. The observer records
writes without changing PCM. Attribution and original terms are preserved in
[AY_EMUL_NOTICE.txt](AY_EMUL_NOTICE.txt) and [THIRD_PARTY.md](../THIRD_PARTY.md).

To investigate regeneration, obtain the matching AY_Emul 3.0 beta sources
separately and place them at the paths recorded in `oracle/provenance.json`.
Check their SHA-256 hashes before using `tools/build_oracle.py` and FPC. The
local developer regression scripts also require those original sources or
compiled adapters. Regeneration is an explicit diagnostic operation, not part
of CI or a normal build. Do not replace fixture expectations with C++ engine
output.

PT2/STC diagnostics use the same chip, mixer and filter adapter plus unchanged
legacy player, initialization and duration routines from `Players.pas`:

```sh
python3 tools/build_legacy_oracle.py --compiler fpc
./build-legacy/oracle-legacy tests/fixtures/legacy/effects.pt2 /tmp/pt2.pcm /tmp/pt2.events.jsonl
```

The generated source remains in the build directory. Observation hooks record
register writes; `ORACLE_TRACE_OFF=1` disables observation for PCM repeatability
checks. Capture buffers use the actual integer IRQ/PCM clocks so long tracks
are not truncated by a nominal-duration allocation. These adapters are
verification tools, not application dependencies.
