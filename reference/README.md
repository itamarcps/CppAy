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
