# Release preparation and publication

Release: **0.1.0**, repository [`itamarcps/CppAy`](https://github.com/itamarcps/CppAy), branch `main`. Full original
AY_Emul scope remains unfinished: this candidate is for the explicitly documented
PT3/PSG/YM3 scope, not an assertion that all original requirements are finished.

## Rebuild and verify

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure --no-tests=error
python3 tools/release_fidelity.py --renderer build-release/aytool --output build-release/public-fidelity
python3 tools/release_fidelity.py --renderer build-release/aytool --output build-release/required-fidelity --private
python3 tools/package_linux.py --build-dir build-release --output build-release/packages
./tools/build_windows.sh --verify
```

Source-reference qualification adds `--oracle reference/oracle/oracle` to the
private gate; see [fidelity setup](FIDELITY.md). Never regenerate golden bytes
with C++Ay. Preserve fixtures, exact renderer/build hashes, notices and failing
reports. Missing private data must fail the required gate, not become a CI skip.
Use a fresh output directory for each evidence run. A changed production source,
compiler/dependency/flag or package binary needs affected checks repeated.

Extract the exact archive outside the source/build tree, use fresh configuration,
remove development Qt/QML/LD overrides and verify the declared runtime model.
Exercise actual-device playback/transport and recompare GUI export. Offscreen
startup only establishes resource/layout checks. Wine is not physical Windows
hardware verification. CI workflow syntax checks are not remotely executed CI.

## Candidate artifacts

The prepared outputs live in `build-release-0.1.0/release/`:

- `C++Ay-0.1.0-linux-x86_64.tar.gz` — system-Qt runtime requirements included.
- `C++Ay-0.1.0-windows-x64.zip` — complete bundled DLL/QML/plugin runtime.
- `C++Ay-0.1.0-source.tar.gz` — public source/fixtures/docs, source identity.
- `C++Ay-0.1.0-public-evidence.tar.gz` — redistributable public corpus evidence.
- `SHA256SUMS`, `release-manifest.json`, `runtime-verification.json` — exact identities/results.

The full original-user-fixture evidence archive stays separately marked private.
Do not upload it: no music redistribution permission is recorded. Small numerical
summaries and the original synthetic listening example are in Git. Original
reference binaries and source collections are not in publication archives.

## Manual publication

For a future release, update the version, rebuild and verify affected gates,
prepare public artifacts and checksums, and review the remaining limitations.
The commands used for 0.1.0 are:

```sh
git push -u origin main
# Only after validating the intended source and published CI:
git tag -a v0.1.0 <verified-release-commit> -m 'C++Ay 0.1.0'
git push origin v0.1.0
```

Then manually create the release against that tag with the notes in
[RELEASE_NOTES.md](RELEASE_NOTES.md), attaching the four public archives and
checksums. Do not upload private audio, local source-reference executables or
machine configuration. No automatic publish-on-push workflow or unverified tag
is used. This focused release is not a complete-AY_Emul release.

Review [LICENSE](../LICENSE), [THIRD_PARTY.md](../THIRD_PARTY.md), original
AY_Emul notice and bundled Windows runtime licenses/SBOMs before publication.
Linux runtime packages are external dependencies, not redistributed components.
