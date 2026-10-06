# Test fixtures

`manifest.json` records immutable hashes for twelve independently generated
synthetic inputs, raw little-endian stereo PCM and ordered register events.
Source-adapter provenance and observer changes are in `reference/oracle`.
They are validated before use; ordinary tests never update expected bytes.

`integration.pt3` is a newly authored 64-row, three-channel test track with a
40-interrupt row delay (2,560 interrupts, approximately 51.2 seconds). Its title
and author fields are synthetic project metadata. It tests long-track seeking,
streaming, controller exports and actual ten-second session checkpoints without
redistributing user music. Its WAV is rendered only inside the build tree and
checks engine integration consistency, not independent compatibility.

All input modules are synthetic. The expected outputs retain source-oracle
attribution described in `THIRD_PARTY.md`.

`legacy/` contains original synthetic PT2 and STC modules and immutable
AY_Emul-derived PCM/register-event references. Its manifest records hashes and
provenance. Normal tests never regenerate them. The STC case covers looping and
finite samples, envelope triggers, ornaments and position transposition; PT2
covers tempo/skip changes, volume/noise, glissando and portamento.
