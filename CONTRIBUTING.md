# Contributing to C++Ay

Build instructions are in [README.md](README.md). Use a 64-bit C++20 toolchain
and Qt 6.8 or later for the desktop player. The core can build without Qt.

Before opening a pull request, run the applicable CTest suite. Keep changes
focused and describe the behavior changed, the checks run, and any remaining
limitations. For UI changes, include a screenshot at the minimum 760×520 window
size and check both three-channel and six-channel songs.

Playback, command-line rendering and WAV export share the native engine. Changes
to decoding, synthesis, interpolation, mixing or filtering must preserve the
immutable PCM/event fixtures in `tests/fixtures`. Do not regenerate expected
outputs using the engine being tested. Describe intentional compatibility changes
and obtain an independent source reference before changing fixture expectations.

The synthetic integration fixture checks controller, streaming and restart
behavior. Its build-generated WAV is a consistency reference, not an independent
audio oracle. Optional private music checks are documented in
[testing](docs/TESTING.md).

Keep SDKs, binaries, local music, logs and generated reports out of commits.
Only submit music or artwork you have permission to redistribute. Original
contributions use the MIT license; preserve all third-party attribution and
notices described in [THIRD_PARTY.md](THIRD_PARTY.md).
