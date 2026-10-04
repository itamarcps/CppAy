# PCM fidelity and reproducibility

The fixed engineering interpretation of the original audio requirement is
complete-interval PCM equivalence. It is not a perceptual accuracy percentage.
The public summary is generated from schema-2 reports by
`tools/publish_fidelity_summary.py`; its `--check` mode detects stale values.

**BIT_EXACT_PCM** requires identical intended rate, precision, channels and
frame count, zero differing decoded PCM values, zero unexplained offset/drift,
and no conflicting independently observed register events when available.
**NEAR_MATCH_TARGET_MET** requires relative RMS error ≤ 1e-6 (residual SNR ≥
120 dB) and maximum absolute error ≤ one reference LSB, separately for every
channel over the whole track, every consecutive nonoverlapping one-second
window including the partial tail, and declared transition/end windows.
Any failing required case fails the aggregate. Empty input fails; matching
prefixes do not pass. Silence requires exact silence. Zero error has infinite
SNR; JSON represents it as null with `positive_infinity`, and silence metrics
are explicitly inapplicable. Measurements use unrounded values.

For reference r, candidate c and error e=c−r:

```
relative_RMS_error = sqrt(sum(e²) / sum(r²))
residual_SNR_dB = 20 log10(RMS(r) / RMS(e))
```

No gate uses gain fitting, normalization, DC removal, channel swaps, polarity
changes, resampling, alignment, time stretching, EQ or selective cropping.
Start/middle/end checks are zero-offset diagnostics. RIFF chunks/padding and
actual bounds are validated; only the exact immutable supplied export hash has
the verified eight-byte oversized RIFF-header exception. New exports are valid
WAV files. The hash exception identifies a container quirk, never playback PCM.

## Reference identity and scope

[Machine-readable fixture/profile manifest](../tests/fixtures/fidelity-manifest.json)
records original pair hashes/sizes, default profile, provenance and unknowns.
The supplied `samples/Flexo02.pt3` / `Flexo02.wav` is an unedited **direct AY_Emul
WAV conversion**, confirmed by the user. The original export executable version,
build and architecture are unknown. The source reference is Sergey Bulba's
AY_Emul **3.0 beta**, with source identities in
[adapter provenance](../reference/oracle/provenance.json) and a reviewable
[observer patch](../reference/oracle/observer.patch).

The fixed source-default YM2149F/1,773,400 Hz/50 Hz profile uses ABC gains
255/13, 170/170, 13/255, preamp 127, 49-tap FIR and source reset/end semantics.
These settings are inferred from source defaults and qualified together against
the entire original export. WAV headers cannot establish them. The user confirmed
later AY_Emul settings screenshots were not the conversion settings and instructed
us to keep this profile. Beeper/DMA gains are inactive on this PT3 path.

The minimal Pascal adapter is independently compiled source code, not the
unknown original export executable. It is rendered twice and with observation;
all three must have identical PCM, and its original-sample output must match
the supplied direct export. Original sources are not altered. Public immutable
fixtures cover selected effects, noise/envelope retriggering, PSG/YM and native
TurboSound; optional source-oracle cases add PT3 header versions/tables and
AY/YM/rate/clock/preamp/FIR/averager profiles. Header mutations are synthetic
cases, not representative real songs. First-natural-end tests do not certify
repeated loop-point continuation or every tracker effect/version.

The supplied direct export has no original independent event trace. A separately
qualified source adapter can provide ordered events; the report states which
trace was used. Unavailable traces stay explicitly unavailable.

The generated integration WAV in CTest comes from C++Ay and establishes
streaming/export consistency only. It is never an independent fidelity oracle.

## Reproduce

Requires Python/NumPy/Matplotlib; compile the optimized shared production core:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure --no-tests=error
python3 tools/release_fidelity.py --renderer build-release/aytool --output build-release/public-fidelity
```

The command checks immutable public fixture hashes, renders/compares complete
intervals and ordered events, then produces per-fixture WAVs, unscaled float32
residuals, JSON reports, min/max overview plots, sample details, signed residual
plots, an HTML listening report, summary, manifest/checksums and `.tar.gz` bundle.
Overview bins retain extrema; metrics over every sample remain authoritative.
The float residual is in normalized reference amplitude at gain 1, without
clipping, including differences beyond unity. It is a derivative, not a gate
input. Exact zero residual is drawn as zero, not fabricated visible noise.

The **required user-fixture release gate** adds the original private pair:

```sh
python3 tools/release_fidelity.py --renderer build-release/aytool --output build-release/required-fidelity --private
```

Missing required music returns a failing exit status and
`BLOCKED_MISSING_REFERENCE` in the manifest. Altered bytes fail immutable
identity verification. Public-only success does not certify this private gate.
Use fresh output directories: no evidence or original golden is overwritten.

If separately built Pascal adapters are available, extend qualification:

```sh
python3 tools/release_fidelity.py --renderer build-release/aytool --output build-release/qualified-fidelity --private --oracle reference/oracle/oracle
```

See [reference setup](../reference/README.md) for source hashes/adapters. This
optional command never regenerates committed expectations. Original music and
its generated audio/trace are **private**: do not distribute that archive.
Public-only bundles contain original synthetic examples, not user music.

## Listen and inspect

The [small public listening example](fidelity/listening/index.html) covers the
entire 0.6400625-second synthetic native TurboSound fixture at unchanged gain.
Numerical comparisons cover full tracks even when presentation is short.
Reference and C++Ay WAV links work independently of GitHub HTML-audio support.
For reliable browser seeking, use the dependency-free range-aware server:

```sh
python3 tools/serve_evidence.py --directory docs/fidelity/listening
# Open http://127.0.0.1:8080/index.html
```

For a generated full bundle, replace the directory with its extracted root.
A/B switches preserve listening position and stop the other audio. Browser/OS
conversion makes this a listening aid, not a measurement of device output.
Optional browser verification requires isolated Playwright plus local Chromium:

```sh
python3 tools/check_evidence_page.py docs/fidelity/listening --report build-release/listening-check.json
```

[All measured rows](fidelity/summary.md) · [small machine-readable summary](fidelity/summary.json).
Large generated WAVs/traces and private audio are excluded from ordinary Git;
only the intentionally small redistributable listening example is tracked.
