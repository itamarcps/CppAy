#!/usr/bin/env python3
"""Fetch pinned Qt/MinGW archives into .deps without changing the host system."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qt-only', action='store_true', help='Use your own cross-compiler; download only the Windows Qt SDK')
    args = parser.parse_args()
    for tool in ['curl', 'tar', '7z']:
        if not shutil.which(tool):
            raise SystemExit(f'Required program missing: {tool}')
    entries = json.loads((ROOT / 'packaging/windows/dependencies.lock.json').read_text())
    if args.qt_only:
        entries = [e for e in entries if e['kind'] == 'qt']
    cache = ROOT / '.deps/downloads'
    cache.mkdir(parents=True, exist_ok=True)

    def fetch(entry):
        path = cache / entry['filename']
        if path.exists() and digest(path) == entry['sha256']:
            return path
        temporary = path.with_suffix(path.suffix + '.part')
        subprocess.run(['curl', '--fail', '--location', '--retry', '3', '--connect-timeout', '15',
                        '--max-time', '900', '--silent', '--show-error', entry['url'], '-o', str(temporary)], check=True)
        if digest(temporary) != entry['sha256']:
            raise RuntimeError(f'Archive SHA-256 mismatch: {entry["filename"]}')
        temporary.replace(path)
        print(f'Downloaded and verified {path.name}', flush=True)
        return path

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        archives = list(pool.map(fetch, entries))
    for entry, archive in zip(entries, archives):
        target = ROOT / '.deps' / ('mingw13' if entry['kind'] == 'arch' else 'qt-windows')
        stamp = target / '.installed' / entry['sha256']
        if stamp.exists():
            continue
        target.mkdir(parents=True, exist_ok=True)
        command = (['tar', '-xf', str(archive), '-C', str(target)] if entry['kind'] == 'arch'
                   else ['7z', 'x', '-y', f'-o{target}', str(archive)])
        subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
        stamp.parent.mkdir(exist_ok=True)
        stamp.touch()
        print(f'Installed {entry["filename"]}', flush=True)
    print('Ready: cmake --preset windows-mingw64')


if __name__ == '__main__':
    main()
