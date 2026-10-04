# Publishing and releases

## Repository publication

The repository is initialized on `main`. Source, branding, synthetic fixtures,
reference provenance and notices are tracked. Build trees, SDK downloads,
private music, reference archives, historical prompts and generated evidence
remain local and are ignored. They are not deleted by repository preparation.

Before publishing, inspect the commit and run the public suite from a fresh
clone. `git ls-files` lists the publication contents. Add the remote URL of your
chosen GitHub repository and push:

```sh
git remote add origin <your-github-repository-url>
git push -u origin main
```

The local preparation does not create a GitHub repository or push any files.
Automatic CI builds/tests the core with sanitizers and the Qt desktop player.
A manually dispatched Windows workflow produces a portable package artifact;
it does not automatically publish releases. Remote Actions results become
available only after pushing to GitHub.

## Release checklist

1. Update the project version in `CMakeLists.txt` and the Windows version resource
   in `packaging/windows/ayplayer.rc`, plus any versioned documentation.
2. Run the public desktop and sanitizer checks. If authorized private golden
   samples are available, run the optional comparison as well.
3. Review minimum-size three/six-channel layouts, actual audio output, seeking,
   compact mixer, List tools, import/export and session restore on the target OS.
4. Build the Windows package with `tools/build_windows.sh`. Use `--verify` for
   Wine qualification, and test physical Windows before claiming hardware support.
5. Review included `LICENSE`, `THIRD_PARTY.md`, `AY_EMUL_NOTICE.txt`, runtime
   notices, SBOMs and dependency/source URLs. Preserve all upstream attribution.
6. Tag the tested commit and attach the ZIP and checksum to a GitHub release.
   Keep packages out of Git history; publish the matching source commit too.

Example local checksum command:

```sh
sha256sum 'build-windows/C++Ay-windows-x64.zip'
```

The packaging script includes a per-file `SHA256SUMS.json` and DLL dependency
manifest inside the ZIP. It includes previous verification only when the
recorded executable hashes match the packaged executables.
