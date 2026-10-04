#!/usr/bin/env python3
"""Check the deployed Windows binaries under an isolated Wine prefix."""
import argparse
import hashlib
import importlib.util
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def windows_path(path):
    return 'Z:' + str(path.resolve()).replace('/', '\\')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, default=ROOT / 'build-windows/C++Ay-windows-x64')
    parser.add_argument('--skip-gui', action='store_true', help='Run engine/export checks without a graphical desktop')
    args = parser.parse_args()
    package = args.package.resolve()
    (ROOT / 'evidence').mkdir(exist_ok=True)
    spec = importlib.util.spec_from_file_location('compare', ROOT / 'tools/compare_pcm.py')
    compare = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(compare)
    env = os.environ.copy()
    for key in ['LD_PRELOAD', 'WINEPATH', 'QT_PLUGIN_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH', 'WAYLAND_DISPLAY']:
        env.pop(key, None)
    env.update(WINEPREFIX=str(ROOT / '.deps/wine-windows'), WINEARCH='win64', WINEDEBUG='-all',
               WINEDLLOVERRIDES='mscoree,mshtml=')
    log = []

    def wine(arguments, cwd=ROOT, required=True):
        # Regular files avoid waiting for pipe EOF from Wine's background services.
        with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
            result = subprocess.run(['wine', *arguments], cwd=cwd, env=env, stdout=stdout, stderr=stderr, timeout=90)
            stdout.seek(0)
            stderr.seek(0)
            result.stdout = stdout.read().decode('utf-8', errors='replace')
            result.stderr = stderr.read().decode('utf-8', errors='replace')
        log.append({'arguments': arguments, 'exit_code': result.returncode,
                    'stdout': result.stdout, 'stderr': result.stderr})
        if required and result.returncode:
            raise RuntimeError(f'Windows command failed ({result.returncode}): {arguments}\n{result.stderr}')
        return result

    if not (ROOT / '.deps/wine-windows/system.reg').exists():
        wine(['wineboot', '-u'])
    # Use only this task's prefix; keep Wine's graphics fallback separate from app defaults.
    wine(['reg', 'add', 'HKCU\\Software\\Wine\\Drivers', '/v', 'Graphics', '/d', 'x11', '/f'])
    # Qt 6.11 requires PKEY_AudioEndpoint_GUID, which Wine 11.18 omits.
    # Supply the existing endpoint's GUID only in this isolated test prefix.
    devices = wine(['reg', 'query', r'HKLM\Software\Microsoft\Windows\CurrentVersion\MMDevices\Audio', '/s'], required=False)
    keys = [line.strip() for line in devices.stdout.splitlines() if line.strip().startswith('HKEY_LOCAL_MACHINE')
            and line.strip().endswith('\\Properties')]
    registry = ['Windows Registry Editor Version 5.00', '']
    for key in keys:
        guid = re.search(r'(\{[0-9A-Fa-f-]+\})\\Properties$', key).group(1)
        registry.extend([f'[{key}]', f'"{{8C7ED206-3F8A-4827-B3AB-AE9E1FAEFC6C}},2"="{guid}"', ''])
    workaround = ROOT / '.deps/wine-audio-endpoints.reg'
    workaround.write_text('\r\n'.join(registry), encoding='utf-16')
    if keys:
        wine(['reg', 'import', windows_path(workaround)])
    report = {'platform': 'Windows x86-64 binaries under Wine on Linux',
              'wine': subprocess.check_output(['wine', '--version'], text=True).strip(),
              'physical_windows_verified': False, 'fixture_cases': [],
              'wine_audio_workaround': 'Populate missing PKEY_AudioEndpoint_GUID in isolated Wine prefix',
              'wine_audio_endpoints': len(keys)}
    report['executables_sha256'] = {}
    for name in ['C++Ay.exe', 'aytool.exe']:
        with (package / name).open('rb') as f:
            report['executables_sha256'][name] = hashlib.file_digest(f, 'sha256').hexdigest()
    fixture_root = ROOT / 'tests/fixtures'
    manifest = json.loads((fixture_root / 'manifest.json').read_text())
    with tempfile.TemporaryDirectory(prefix='windows-verify-', dir=ROOT / 'build-windows') as temporary:
        work = Path(temporary)
        tool = windows_path(package / 'aytool.exe')
        golden = work / 'golden.wav'
        private_source = ROOT / 'samples/Flexo02.pt3'
        private_wav = ROOT / 'samples/Flexo02.wav'
        has_golden = private_source.exists() and private_wav.exists()
        input_track = private_source if has_golden else fixture_root / 'integration.pt3'
        wine([tool, 'render', windows_path(input_track), windows_path(golden)])
        actual_meta, actual = compare.read_wav(golden)
        if has_golden:
            expected_meta, expected = compare.read_wav(private_wav)
            golden_report = compare.compare(expected,actual,expected_meta,actual_meta)
            if golden_report['status'] not in compare.ACCEPTED:
                raise AssertionError('Windows full-song golden PCM/metadata mismatch')
            report['golden_pcm'] = {'status': 'BIT_EXACT_PCM', 'stereo_frames': len(actual)}
        else:
            # Integration consistency only; immutable independent fixtures below
            # remain the compatibility oracle for a public checkout.
            expected = actual.copy(); expected_meta = actual_meta.copy()
            report['golden_pcm'] = {'status': 'NOT_RUN', 'reason': 'Private music/WAV pair not supplied'}
            report['integration_reference'] = {'source': 'Windows CLI render of synthetic integration.pt3',
                                               'stereo_frames': len(actual)}
        for case in manifest['cases']:
            name = case['input']
            for filename, digest in case['files'].items():
                with (fixture_root / filename).open('rb') as f:
                    if hashlib.file_digest(f, 'sha256').hexdigest() != digest:
                        raise AssertionError(f'Immutable fixture changed: {filename}')
            candidate, trace = work / (name + '.wav'), work / (name + '.jsonl')
            wine([tool, 'render', windows_path(fixture_root / name), windows_path(candidate)])
            wine([tool, 'trace', windows_path(fixture_root / name), windows_path(trace)])
            _, actual = compare.read_wav(candidate)
            if actual.astype('<i2').tobytes() != (fixture_root / (name + '.pcm')).read_bytes():
                raise AssertionError(f'Windows independent PCM mismatch: {name}')
            if compare.compare_events(fixture_root / (name + '.reference.jsonl'), trace)['status'] != 'EXACT_ORDERED_EVENTS':
                raise AssertionError(f'Windows independent events mismatch: {name}')
            report['fixture_cases'].append({'input': name, 'pcm': 'BIT_EXACT_PCM', 'events': 'EXACT_ORDERED_EVENTS'})
            print(f'{name}: BIT_EXACT_PCM + EXACT_ORDERED_EVENTS', flush=True)
        unicode_dir = work / 'Música 日本語'
        unicode_dir.mkdir()
        source, output = unicode_dir / 'canção.pt3', unicode_dir / 'áudio.wav'
        shutil.copy2(input_track, source)
        wine([tool, 'render', windows_path(source), windows_path(output)])
        if output.read_bytes() != golden.read_bytes():
            raise AssertionError('Unicode Windows input/output mismatch')
        saved_hash = hashlib.sha256(output.read_bytes()).hexdigest()
        refused = wine([tool, 'render', windows_path(source), windows_path(output)], required=False)
        if not refused.returncode or hashlib.sha256(output.read_bytes()).hexdigest() != saved_hash:
            raise AssertionError('Windows export overwrote an existing file')
        report['unicode_paths'] = 'PASS'
        report['overwrite_refused'] = 'PASS'
        if not args.skip_gui:
            (work / 'evidence').mkdir()

            def gui(mode, inputs):
                # Exercise the application's production rendering defaults.
                command = ' '.join('"' + a.replace('%', '%%') + '"' for a in
                                   [windows_path(package / 'C++Ay.exe'), mode, *map(windows_path, inputs)])
                batch = work / 'gui-test.cmd'
                batch.write_text('@echo off\nset QT_FORCE_STDERR_LOGGING=1\n' +
                                 'start "" /wait ' + command + '\nexit /b %errorlevel%\n', encoding='utf-8')
                return wine(['cmd', '/c', windows_path(batch)], cwd=work, required=False)

            result = gui('--smoke-test', [input_track, fixture_root / 'native-ts.pt3'])
            smoke_path = work / 'evidence/gui-smoke.json'
            if not smoke_path.exists():
                (ROOT / 'build-windows/wine-verification-log.json').write_text(json.dumps(log, indent=2))
                raise AssertionError(f'Windows GUI did not complete: {result.stderr}')
            report['gui'] = json.loads(smoke_path.read_text())
            report['gui']['exit_code'] = result.returncode
            gui_meta, actual = compare.read_wav(work / 'evidence/gui-export.wav')
            gui_report=compare.compare(expected,actual,expected_meta,gui_meta)
            if gui_report['status'] not in compare.ACCEPTED:
                raise AssertionError('Windows GUI WAV export differs from the CLI/reference PCM')
            report['gui_export_pcm'] = gui_report['status']
            report['gui_export_measurements'] = gui_report
            shutil.copy2(work / 'evidence/ui.png', ROOT / 'evidence/windows-ui.png')
            result = gui('--stream-smoke-test', [input_track, fixture_root / 'native-ts.pt3'])
            stream_path = work / 'evidence/stream-device-smoke.json'
            if not stream_path.exists():
                raise AssertionError(f'Windows streaming smoke did not complete: {result.stderr}')
            report['streaming'] = json.loads(stream_path.read_text())
            report['streaming']['exit_code'] = result.returncode
            result = gui('--layout-smoke-test', [input_track, fixture_root / 'native-ts.pt3'])
            layout_path = work / 'evidence/ui-layout.json'
            if result.returncode or not layout_path.exists():
                raise AssertionError(f'Windows layout smoke failed: {result.stderr}')
            report['layout'] = json.loads(layout_path.read_text())
            shutil.copy2(layout_path, ROOT / 'evidence/windows-ui-layout.json')
            report['wine_rendering'] = 'Application-default Qt Quick software backend; isolated prefix uses X11'
            result = gui('--window-smoke-test', [])
            window_path = work / 'evidence/window-controls.json'
            if result.returncode or not window_path.exists():
                raise AssertionError(f'Windows custom window controls failed: exit={result.returncode}, report={window_path.read_text() if window_path.exists() else "missing"}, stderr={result.stderr}')
            report['window_controls'] = json.loads(window_path.read_text())
            if report['window_controls']['status'] != 'PASS':
                raise AssertionError('Windows custom title-bar checks failed')
    report['status'] = 'PASS' if args.skip_gui or (report['gui']['exit_code'] == 0 and report['streaming']['exit_code'] == 0) else 'ENGINE_AND_GUI_EXPORT_PASS_AUDIO_UNVERIFIED'
    with (ROOT / 'evidence/windows-verification.json').open('w') as f:
        json.dump(report, f, indent=2)
        f.write('\n')
    (ROOT / 'build-windows/wine-verification-log.json').write_text(json.dumps(log, indent=2) + '\n')
    print(report['status'])
    if report['status'] != 'PASS':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
