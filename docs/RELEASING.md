# Preparing a release

Releases are published manually to [itamarcps/CppAy](https://github.com/itamarcps/CppAy).

## Build and verify

Update the application version and changelog, then build an optimized release:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure --no-tests=error
python3 tools/release_fidelity.py --renderer build-release/aytool --output build-release/public-fidelity
python3 tools/package_linux.py --build-dir build-release --output build-release/packages
./tools/build_windows.sh --verify
```

Use fresh package and evidence directories. The additional private-reference
qualification is described in [FIDELITY.md](FIDELITY.md); missing required data
must fail its gate. Never generate golden expectations using C++Ay.

Extract the exact packages outside the build tree and check startup, resources,
playback, seeking, mixer settings and WAV export with isolated settings. Remove
development Qt/QML path overrides. Record unavailable devices and platforms;
Wine and offscreen checks do not establish physical Windows/audio verification.

## Publish

Verify CI and artifact checksums before tagging the tested revision. Upload the
Linux and Windows packages, source snapshot, public audio evidence and
`SHA256SUMS`. Use [RELEASE_NOTES.md](RELEASE_NOTES.md) as the release description.
Keep internal machine reports, private music and reference binaries local.

The source snapshot must identify its revision. Documentation-only refreshes
may use a newer snapshot than the binaries; record both identities rather than
moving an existing release tag. Any audio, dependency or compiler-option change
requires the affected verification again.

Retain [component notices](../THIRD_PARTY.md) and dependency licenses inside
packages. Linux runtime libraries are external dependencies; Windows includes
its declared Qt/MinGW runtime. Review installation requirements and supported
formats before publishing. CI does not publish releases automatically.
