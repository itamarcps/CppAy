#!/usr/bin/env python3
"""Linux x86-64 binary archive using declared system Qt/runtime dependencies."""
import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import tarfile
from release_fidelity import source_identity
ROOT=pathlib.Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build-dir',type=pathlib.Path,required=True)
    p.add_argument('--output',type=pathlib.Path,required=True)
    a=p.parse_args();build=a.build_dir.resolve();output=a.output.resolve();output.mkdir(parents=True,exist_ok=True)
    if 'CMAKE_BUILD_TYPE:STRING=Release' not in (build/'CMakeCache.txt').read_text():
        p.error('Release build required')
    env=os.environ.copy()
    for key in ('LD_PRELOAD','LD_LIBRARY_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH'):
        env.pop(key,None)
    version=subprocess.check_output([build/'aytool','--version'],text=True,env=env).strip().split()[-1]
    name='C++Ay-'+version+'-linux-x86_64';stage=output/name
    if stage.exists():p.error('Use a fresh output directory; refuse replacing package')
    subprocess.run(['cmake','--install',build,'--prefix',stage],check=True,env=env)
    # Qt's scanner verifies actual system QML imports used by this dependency
    # model. No Qt/host libraries are copied, no suppressed deploy no-op.
    cache=(build/'CMakeCache.txt').read_text()
    qt_dir=next(line.split('=',1)[1] for line in cache.splitlines() if line.startswith('Qt6_DIR:PATH='))
    prefix=pathlib.Path(qt_dir).parents[2]
    scanner=next((v for v in (prefix/'lib/qt6/qmlimportscanner',prefix/'libexec/qmlimportscanner',prefix/'bin/qmlimportscanner') if v.exists()),None)
    qml=next((v for v in (prefix/'lib/qt6/qml',prefix/'qml') if v.exists()),None)
    if scanner is None or qml is None:raise RuntimeError('Declared Qt installation lacks QML scanner/runtime')
    imports=json.loads(subprocess.check_output([scanner,'-rootPath',ROOT/'qml','-importPath',qml],env=env,text=True))
    modules=[]
    for entry in imports:
        if entry.get('type')=='module':
            path=pathlib.Path(entry.get('path',''))
            if not entry.get('path') or not (path/'qmldir').is_file():raise RuntimeError('Missing QML import '+entry.get('name','?'))
            plugin=entry.get('plugin')
            if plugin and not (path/('lib'+plugin+'.so')).is_file():raise RuntimeError('Missing QML plugin '+plugin)
            modules.append({k:entry[k] for k in ('name','plugin','relativePath') if k in entry})
    dependencies={}
    symbol_requirements={}
    for binary in ('C++Ay','aytool'):
        path=stage/'bin'/binary
        ldd=subprocess.check_output(['ldd',path],text=True,env=env)
        if 'not found' in ldd:raise RuntimeError('Undeclared missing runtime dependency')
        dependencies[binary]=ldd
        dynamic=subprocess.check_output(['readelf','-d',path],text=True,env=env)
        runtime_paths=re.findall(r'\((?:RUNPATH|RPATH)\).*?\[(.*?)\]',dynamic)
        if any(part and not part.startswith('$ORIGIN') for paths in runtime_paths for part in paths.split(':')):
            raise RuntimeError('Binary embeds an absolute/development runtime search path')
        versions=subprocess.check_output(['readelf','--version-info',path],text=True,env=env)
        symbol_requirements[binary]=sorted(set(re.findall(r'(?:GLIBCXX|GLIBC|CXXABI)_[0-9.]+',versions)))
    packages={}
    if shutil.which('pacman'):
        for package in ('qt6-base','qt6-declarative','qt6-multimedia','gcc-libs','glibc','ffmpeg','libpulse','pipewire-pulse'):
            r=subprocess.run(['pacman','-Q',package],capture_output=True,text=True,env=env)
            if r.returncode==0:packages[package]=r.stdout.strip()
    info={'version':version,'architecture':'x86-64','dependency_model':'system runtime; no Qt or host libraries bundled',
        'source_revision':source_identity()[0],
        'build_cache_sha256':sha(build/'CMakeCache.txt'),'binary_sha256':{b:sha(stage/'bin'/b) for b in ('C++Ay','aytool')},
        'system_qml_modules':modules,'linked_dependencies':dependencies,'required_symbol_versions':symbol_requirements,
        'tested_runtime_packages':packages,'runtime_test':'NOT_EXECUTED: verify extracted archive separately'}
    (stage/'PACKAGE.json').write_text(json.dumps(info,indent=2)+'\n')
    (stage/'INSTALL.txt').write_text('C++Ay '+version+' Linux x86-64\n\nExtract the whole archive; run bin/C++Ay.\nThis is a dynamically linked system-Qt build, not a universal portable bundle.\nRequires compatible Qt 6.11.2 runtime: qt6-base, qt6-declarative, qt6-multimedia,\nthe Basic Qt Quick Controls style, Qt Quick Dialogs and Layouts QML modules,\nWayland or XCB platform plugin, audio backend and its sound-server libraries.\nFor the prepared CachyOS build, use matching CachyOS/Arch runtime versions\nlisted in PACKAGE.json (including GCC runtime and glibc symbol requirements).\nNo Qt development installation is needed, but runtime packages are required.\nEmbedded application QML, branding and icons are in the executable.\nSystem fonts, Qt translations and platform theme come from the declared runtime.\nOptional KDE native dialogs require plasma-integration.\n\nTry bin/aytool --version or bin/C++Ay --version.\nFirst use: Add… a module, double-click its row, adjust Mixer, Export WAV…\nProject/AY_Emul notices: share/doc/cppay/. No third-party runtime is distributed.\n')
    sums={str(f.relative_to(stage)):sha(f) for f in sorted(stage.rglob('*')) if f.is_file()}
    (stage/'SHA256SUMS.json').write_text(json.dumps(sums,indent=2)+'\n')
    archive=output/(name+'.tar.gz')
    with tarfile.open(archive,'w:gz') as tar:tar.add(stage,arcname=name)
    print(archive)
if __name__=='__main__':main()
