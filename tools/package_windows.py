#!/usr/bin/env python3
"""Deploy Windows Qt/QML and recursive PE dependencies using Linux host tools."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
# DLLs provided by supported Windows versions; never copy host system DLLs.
SYSTEM_DLLS = set('advapi32 authz avicap32 avrt bcrypt cabinet cfgmgr32 comdlg32 crypt32 cryptbase '
                 'd2d1 d3d11 d3d12 d3d9 dcomp dbghelp dnsapi dsound dwmapi dwrite dxgi dxguid dxva2 evr gdi32 glu32 '
                 'hid imagehlp imm32 iphlpapi kernel32 ksuser mf mfplat mfreadwrite mfuuid mmdevapi mpr '
                 'msacm32 msvcrt ncrypt netapi32 ntdll ole32 oleaut32 opengl32 powrprof propsys psapi '
                 'rpcrt4 secur32 setupapi shcore shell32 shlwapi strmiids user32 userenv usp10 uxtheme '
                 'winspool.drv version winhttp wininet winmm winspool wintrust wldap32 ws2_32 wtsapi32'.split())


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build-windows')
    parser.add_argument('--qt', type=Path, default=ROOT / '.deps/qt-windows')
    parser.add_argument('--mingw', type=Path, default=ROOT / '.deps/mingw13/usr')
    parser.add_argument('--qt-host', type=Path, default=Path(os.environ.get('QT_HOST_PATH', '/usr')))
    args = parser.parse_args()
    build, qt, mingw = args.build_dir.resolve(), args.qt.resolve(), args.mingw.resolve()
    objdump = mingw / 'bin/x86_64-w64-mingw32-objdump'
    scanner = next((p for p in [args.qt_host / 'lib/qt6/qmlimportscanner', args.qt_host / 'libexec/qmlimportscanner',
                               args.qt_host / 'bin/qmlimportscanner'] if p.is_file()), None)
    if scanner is None:
        raise SystemExit('Native Qt qmlimportscanner not found; pass --qt-host')
    for name in ['C++Ay.exe', 'aytool.exe']:
        if not (build / name).is_file():
            raise SystemExit(f'Build first: {build / name}')
    imports = json.loads(subprocess.check_output([str(scanner), '-rootPath', str(ROOT / 'qml'), '-importPath', str(qt / 'qml')], text=True))
    module_paths = {Path(i['path']).resolve() for i in imports if i.get('path')}
    build.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='windows-deploy-', dir=build) as temporary:
        stage = Path(temporary) / 'C++Ay-windows-x64'
        stage.mkdir()
        for name in ['C++Ay.exe', 'aytool.exe']:
            shutil.copy2(build / name, stage / name)
        # Keep the QML import closure, including Basic and optional Controls styles.
        def ignore(directory, names):
            parent = Path(directory)
            return [name for name in names if name.endswith(('.qmltypes', '.pdb', '.debug', '.a', '.lib', '.prl'))
                    or ((parent / name).is_dir() and (parent / name / 'qmldir').exists()
                        and (parent / name).resolve() not in module_paths)]
        for module in sorted(module_paths):
            shutil.copytree(module, stage / 'qml' / module.relative_to(qt / 'qml'), dirs_exist_ok=True, ignore=ignore)
        for category in ['platforms', 'imageformats', 'iconengines', 'multimedia', 'networkinformation', 'tls', 'styles']:
            # Qt's OpenSSL plugin needs an optional external OpenSSL install; Schannel uses Windows.
            for plugin in (qt / 'plugins' / category).glob('*.dll'):
                if category == 'platforms' and plugin.name != 'qwindows.dll':
                    continue
                if plugin.name == 'qopensslbackend.dll':
                    continue
                destination = stage / 'plugins' / category / plugin.name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(plugin, destination)
        (stage / 'qt.conf').write_text('[Paths]\nPrefix=.\nPlugins=plugins\nQmlImports=qml\nTranslations=translations\n', encoding='utf-8')
        # GCC runtime must match the compiler, rather than accidentally using a different SDK runtime.
        search = [mingw / 'x86_64-w64-mingw32/bin', qt / 'bin', qt]
        available = {p.name.lower(): p for folder in reversed(search) for p in folder.glob('*.dll')}
        queue = list(stage.rglob('*.dll')) + list(stage.glob('*.exe'))
        for optional in ['d3dcompiler_47.dll', 'opengl32sw.dll']:
            if optional in available:
                shutil.copy2(available[optional], stage / optional)
                queue.append(stage / optional)
        checked, dependencies = set(), {}
        while queue:
            binary = queue.pop()
            if binary in checked:
                continue
            checked.add(binary)
            info = subprocess.check_output([str(objdump), '-p', str(binary)], text=True)
            if 'file format pei-x86-64' not in info:
                raise RuntimeError(f'Not a Windows x86-64 binary: {binary}')
            names = re.findall(r'DLL Name: (\S+)', info)
            dependencies[str(binary.relative_to(stage))] = names
            for name in names:
                lower = name.lower()
                if lower.startswith(('api-ms-', 'ext-ms-')) or lower.removesuffix('.dll') in SYSTEM_DLLS:
                    continue
                if lower not in available:
                    raise RuntimeError(f'Missing runtime DLL: {name} required by {binary.name}')
                destination = stage / available[lower].name
                if not destination.exists():
                    shutil.copy2(available[lower], destination)
                    queue.append(destination)
        for translation in (qt / 'translations').glob('qtbase_*.qm'):
            target = stage / 'translations' / translation.name
            target.parent.mkdir(exist_ok=True)
            shutil.copy2(translation, target)
        shutil.copy2(ROOT / 'THIRD_PARTY.md', stage / 'THIRD_PARTY.md')
        shutil.copy2(ROOT / 'LICENSE', stage / 'LICENSE')
        shutil.copy2(ROOT / 'reference/AY_EMUL_NOTICE.txt', stage / 'AY_EMUL_NOTICE.txt')
        shutil.copy2(ROOT / 'packaging/windows/README.txt', stage / 'README.txt')
        # Bundle notices and the SDK SBOMs; supplied reference music is not distributed.
        verification = ROOT / 'evidence/windows-verification.json'
        if verification.exists():
            qualification = json.loads(verification.read_text())
            if qualification.get('executables_sha256') == {name: digest(stage / name) for name in ['C++Ay.exe', 'aytool.exe']}:
                shutil.copy2(verification, stage / 'verification.json')
        shutil.copytree(ROOT / 'packaging/windows/licenses', stage / 'licenses')
        shutil.copy2(ROOT / 'packaging/windows/dependencies.lock.json', stage / 'licenses/dependencies.lock.json')
        shutil.copytree(qt / 'sbom', stage / 'licenses/qt-sbom')
        (stage / 'runtime-dependencies.json').write_text(json.dumps(dependencies, indent=2) + '\n')
        manifest = {str(p.relative_to(stage)): digest(p)
                    for p in sorted(stage.rglob('*')) if p.is_file()}
        (stage / 'SHA256SUMS.json').write_text(json.dumps(manifest, indent=2) + '\n')
        # Replace only the script-owned deployment directory after complete dependency validation.
        destination = build / stage.name
        if destination.exists():
            shutil.rmtree(destination)
        shutil.move(str(stage), destination)
    archive = build / (destination.name + '.zip')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for file in sorted(destination.rglob('*')):
            if file.is_file():
                z.write(file, file.relative_to(build))
    print(f'Packaged {len(dependencies)} Windows x64 binaries: {archive} ({archive.stat().st_size / 1024**2:.1f} MiB)')


if __name__ == '__main__':
    main()
