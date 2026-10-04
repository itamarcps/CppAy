#!/usr/bin/env python3
"""Render, measure, plot and bundle actual Release PCM. Missing required data fails."""
import argparse
import datetime
import hashlib
import html
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tarfile
import wave
import numpy as np
import compare_pcm as pcm

ROOT = pathlib.Path(__file__).resolve().parents[1]
def digest(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
def source_identity():
    if (ROOT/'.git').exists():
        revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
        patch=hashlib.sha256(subprocess.check_output(['git','diff','HEAD','--binary'],cwd=ROOT)).hexdigest()
        return revision,patch
    identity=ROOT/'SOURCE_IDENTITY.json'
    return (json.loads(identity.read_text())['revision'] if identity.exists() else 'unversioned-source',None)

def save(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False)+'\n')
def run(cmd, commands, env=None):
    commands.append(list(map(str, cmd)))
    result = subprocess.run(cmd, capture_output=True, text=True, env=env)
    if result.returncode:
        raise RuntimeError('Command failed: '+repr(cmd)+'\n'+result.stdout+result.stderr)
    return result.stdout.strip()
def wrap_raw(source, target, rate=48000):
    data = source.read_bytes()
    if not data or len(data)%4:
        raise ValueError('invalid independent raw stereo PCM')
    with wave.open(str(target), 'wb') as w:
        w.setparams((2,2,rate,0,'NONE','not compressed')); w.writeframes(data)
def plot(folder, r, c, rate, title):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    plt.rcParams.update({'axes.facecolor':'#182127','figure.facecolor':'#252e37',
        'text.color':'#dee4ea','axes.labelcolor':'#dee4ea','xtick.color':'#aab6c2',
        'ytick.color':'#aab6c2','axes.edgecolor':'#7b8793','font.size':9})
    fig, axes = plt.subplots(3,1,figsize=(10,6), constrained_layout=True)
    # Min/max envelope, never decimation: preserve extrema in overview bins.
    starts = np.arange(0,len(r),max(1,len(r)//1800))
    for a,label,color in ((r,'AY_Emul','#e7b456'),(c,'C++Ay','#79b6c7')):
        channel=a[:,0];t=starts/rate
        lo=np.minimum.reduceat(channel,starts);hi=np.maximum.reduceat(channel,starts)
        axes[0].fill_between(t,lo,hi,color=color,alpha=.5,label=label)
    axes[0].set(title=title+' · channel L overview',ylabel='PCM steps (16-bit)',xlabel='Seconds')
    axes[0].legend(loc='upper right')
    unequal=np.flatnonzero(np.any(r!=c,axis=1))
    begin=max(0,int(unequal[0])-20) if len(unequal) else min(rate//10,max(0,len(r)-100))
    end=min(len(r),begin+100);x=np.arange(begin,end)
    axes[1].plot(x,r[begin:end,0],color='#e7b456',label='AY_Emul',linewidth=2)
    axes[1].plot(x,c[begin:end,0],color='#79b6c7',linestyle='--',label='C++Ay')
    axes[1].set(title=f'Sample detail: frames [{begin}, {end}) · same scale, gain 1',ylabel='PCM steps',xlabel='Frame')
    difference=c.astype(np.int64)-r.astype(np.int64)
    # Residual min/max across both channels, retains the worst sample.
    lo=np.minimum.reduceat(difference.min(axis=1),starts)
    hi=np.maximum.reduceat(difference.max(axis=1),starts)
    axes[2].fill_between(starts/rate,lo,hi,color='#e7b456',alpha=.5)
    axes[2].plot(starts/rate,lo,color='#e7b456',linewidth=.8)
    if not np.any(difference):
        axes[2].set_ylim(-1,1)
        axes[2].text(.5,.75,'Exact zero residual — both channels',transform=axes[2].transAxes,ha='center')
    axes[2].set(title='Signed residual, both channels · gain 1 · complete interval',ylabel='Reference LSB',xlabel='Seconds')
    fig.savefig(folder/'comparison.png',dpi=150);plt.close(fig)

def table(reports):
    lines=['| Fixture / profile | PCM | Frames (seconds) | Unequal samples | Max error (LSB) | Worst window relative / SNR | Verdict |',
           '| --- | --- | ---: | ---: | ---: | --- | --- |']
    for identity,r in reports:
        if r['status'] not in pcm.ACCEPTED and 'channels' not in r:
            lines.append(f'| {identity} | — | — | — | — | — | {r["status"]} |');continue
        worst=r['worst_window']['metrics'];relative=worst['relative_rms_error']
        value='N/A (silence)' if relative is None else format(relative,'.9g')
        snr='∞ (zero error)' if worst['snr_status']=='positive_infinity' else 'N/A (silence)' if worst['residual_snr_db'] is None else format(worst['residual_snr_db'],'.9g')+' dB'
        maximum=max(m['max_abs_error_lsb'] for m in r['channels'])
        lines.append(f'| {identity} | {r["reference"]["rate_hz"]} Hz / {r["reference"]["bits"]}-bit / {r["reference"]["channels"]} ch | {r["coverage_frames"]:,} ({r["duration_seconds"]:.7f}) | {r["unequal_samples"]} | {maximum} | {value} / {snr} | **{r["status"]}** |')
    return '\n'.join(lines)+'\n'

def page(out, reports):
    sections=[]
    for identity,r in reports:
        name=r['artifact_directory'];label=html.escape(identity)
        sections.append(f'''<section><h2>{label}</h2><p>{html.escape(r['status'])} · complete PCM interval; gain 1.</p>
        <img src="{name}/comparison.png" alt="Measured PCM and signed residual for {label}">
        <p><a href="{name}/reference.wav">Reference WAV</a> · <a href="{name}/candidate.wav">C++Ay WAV</a> · <a href="{name}/difference.wav">Unscaled float residual</a> · <a href="{name}/report.json">Measurements</a></p>
        <div class="ab"><button type="button" data-side="reference">Listen to reference</button>
        <button type="button" data-side="candidate">Listen to C++Ay</button><button type="button" data-side="pause">Pause</button>
        <audio controls preload="metadata" src="{name}/reference.wav" data-side="reference"></audio>
        <audio controls preload="metadata" src="{name}/candidate.wav" data-side="candidate"></audio></div></section>''')
    out.joinpath('index.html').write_text('''<!doctype html><html lang="en"><meta charset="utf-8"><link rel="icon" href="data:,"><title>C++Ay PCM evidence</title>
    <style>body{background:#252e37;color:#dee4ea;font:16px system-ui;margin:2em auto;max-width:1100px}a{color:#e7b456}img{width:100%}section{padding:1em;border:1px solid #79828c;margin:1em 0}audio{display:block;width:100%}button{padding:.6em}</style>
    <h1>C++Ay independent PCM evidence</h1><p>Offline listening aid; browser/OS playback may convert audio. Numerical reports compare original decoded PCM. Reference and candidate cover the same complete interval at unchanged gain. Residual is IEEE float32, gain 1, including values beyond unity without clipping.</p>
    <p><a href="manifest.json">Provenance manifest</a> · <a href="summary.md">Results table</a></p>'''+''.join(sections)+'''
    <script>
    document.querySelectorAll('.ab').forEach(group=>{
      const tracks=[...group.querySelectorAll('audio')];let selected=tracks[0];
      tracks.forEach(track=>track.addEventListener('play',()=>{selected=track;tracks.forEach(other=>{if(other!==track)other.pause()})}));
      group.querySelectorAll('button').forEach(button=>button.addEventListener('click',()=>{
        const active=tracks.find(t=>!t.paused)||selected;
        const position=active.currentTime;tracks.forEach(t=>t.pause());
        const target=tracks.find(t=>t.dataset.side===button.dataset.side);
        if(target){selected=target;target.currentTime=Math.min(position,target.duration||position);target.play().catch(()=>{});}
      }));
    });
    </script></html>''')

def aggregate_result(reports, required_gate, error=None):
    passed=bool(reports) and not error and all(r['status'] in pcm.ACCEPTED for _,r in reports)
    if passed:return 'PASS'
    blocked=required_gate.get('status','')
    return blocked if blocked.startswith('BLOCKED_') else 'MISMATCH'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--renderer',type=pathlib.Path,required=True)
    p.add_argument('--output',type=pathlib.Path,required=True)
    p.add_argument('--build-dir',type=pathlib.Path,help='Release build identity when testing an installed renderer')
    p.add_argument('--private',action='store_true',help='Required full release gate: fail if supplied pair missing')
    p.add_argument('--oracle',type=pathlib.Path,help='Qualified Pascal adapter; also requires adjacent oracle-observed')
    args=p.parse_args();out=args.output.resolve();renderer=args.renderer.resolve()
    out.mkdir(parents=True,exist_ok=True)
    if (out/'manifest.json').exists():
        p.error('Use a fresh output directory; evidence is never silently replaced')
    manifest=json.loads((ROOT/'tests/fixtures/fidelity-manifest.json').read_text())
    commands=[];reports=[]
    inputs=[ROOT/'CMakeLists.txt',ROOT/'CMakePresets.json']
    for subdir in ('src','qml','assets','cmake','tools','packaging','tests','reference/oracle'):
        inputs.extend(f for f in (ROOT/subdir).rglob('*') if f.is_file() and not any(part=='__pycache__' for part in f.parts) and f.suffix not in ('.o','.ppu') and (subdir!='reference/oracle' or f.suffix in ('.pas','.json','.patch')))
    input_hashes={str(f.relative_to(ROOT)):digest(f) for f in sorted(set(inputs))}
    manifest['candidate_inputs_sha256']=hashlib.sha256(json.dumps(input_hashes,sort_keys=True).encode()).hexdigest()
    manifest['candidate_input_files']=input_hashes
    manifest['measurement_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
    revision,patch=source_identity()
    manifest['candidate']={'renderer_sha256':digest(renderer),'renderer_version':run([renderer,'--version'],commands),
        'source_revision':revision,
        'working_tree_diff_sha256':patch,
        'build_configuration':'Release; shared production aycore', 'renderer_path':str(renderer)}
    build=args.build_dir.resolve() if args.build_dir else renderer.parent
    cache=build/'CMakeCache.txt'
    if args.build_dir and (not cache.is_file() or digest(build/'aytool')!=digest(renderer)):
        p.error('Installed renderer does not match the declared production build')
    if cache.exists():
        if 'CMAKE_BUILD_TYPE:STRING=Release' not in cache.read_text():
            p.error('Fidelity demonstration requires an optimized Release build')
        manifest['candidate']['cmake_cache_sha256']=digest(cache)
        for line in cache.read_text().splitlines():
            if line.startswith(('CMAKE_CXX_COMPILER:FILEPATH=','CMAKE_CXX_FLAGS_RELEASE:STRING=','Qt6_DIR:PATH=','CMAKE_BUILD_TYPE:STRING=')):
                key,value=line.split('=',1);manifest['candidate'][key]=value
        compiler=manifest['candidate'].get('CMAKE_CXX_COMPILER:FILEPATH')
        if compiler:manifest['candidate']['compiler_version']=run([compiler,'--version'],commands).splitlines()[0]
        qt_dir=manifest['candidate'].get('Qt6_DIR:PATH')
        if qt_dir:
            version_file=pathlib.Path(qt_dir)/'Qt6ConfigVersionImpl.cmake'
            if version_file.exists():
                import re
                match=re.search(r'set\(PACKAGE_VERSION \"(.*?)\"\)',version_file.read_text())
                manifest['candidate']['qt_version']=match.group(1) if match else 'See Qt6_DIR'
    fixtures=json.loads((ROOT/'tests/fixtures/manifest.json').read_text())
    def check_file(entry):
        path=ROOT/entry['path']
        if not path.exists():raise FileNotFoundError(entry['path'])
        if path.stat().st_size!=entry['size'] or digest(path)!=entry['sha256']:
            raise ValueError('Immutable reference identity differs: '+entry['path'])
        return path
    def measure(name,source,reference,options=(),events=None,profile=None):
        folder=out/name;folder.mkdir()
        candidate=folder/'candidate.wav';target=folder/'reference.wav'
        shutil.copyfile(reference,target)
        run([renderer,'render',source,candidate,*options],commands)
        rm,r=pcm.read_wav(target);cm,c=pcm.read_wav(candidate)
        width=max(1,rm['rate_hz']//50);mid=len(r)//2
        bounds=sorted(set([(0,min(width,len(r))), (mid,min(mid+width,len(r))), (max(0,len(r)-width),len(r))]))
        report=pcm.compare(r,c,rm,cm,bounds)
        if events:
            actual=folder/'candidate.jsonl';run([renderer,'trace',source,actual,*options],commands)
            shutil.copyfile(events,folder/'reference.jsonl')
            report['events']=pcm.compare_events(events,actual)
            report['event_trace_status']=report['events']['status']
            if report['event_trace_status']!='EXACT_ORDERED_EVENTS':
                report.update(pcm_status=report['status'],status='MISMATCH',reason='CONFLICTING_EVENTS')
        report['fixture']=name;report['artifact_directory']=name
        report['profile']=profile or manifest['profile_version']
        report['input']={'path':str(source.relative_to(ROOT)) if source.is_relative_to(ROOT) else source.name,
                         'sha256':digest(source),'size':source.stat().st_size}
        if report.get('coverage_frames'):
            pcm.write_difference(folder/'difference.wav',r,c,rm)
            plot(folder,r,c,rm['rate_hz'],name)
        save(folder/'report.json',report);reports.append((name,report))
    try:
        for case in fixtures['cases']:
            for name,sha in case['files'].items():
                if digest(ROOT/'tests/fixtures'/name)!=sha:raise ValueError('Immutable public fixture differs: '+name)
            name=case['input'];raw=ROOT/'tests/fixtures'/(name+'.pcm')
            ref=out/('reference-'+name+'.wav');wrap_raw(raw,ref)
            measure(name,ROOT/'tests/fixtures'/name,ref,events=ROOT/'tests/fixtures'/(name+'.reference.jsonl'))
            ref.unlink()
        if args.private:
            entry=manifest['required_private']
            try:source=check_file(entry['input']);ref=check_file(entry['reference'])
            except FileNotFoundError as e:
                manifest['required_gate']={'status':'BLOCKED_MISSING_REFERENCE','missing':str(e)}
                raise
            measure('Flexo02',source,ref)
            manifest['required_gate']={'status':reports[-1][1]['status'],'redistribution':'PRIVATE'}
        else:
            manifest['required_gate']={'status':'NOT_EXECUTED','reason':'Public subset; use --private for required user-fixture gate'}
        if args.oracle:
            oracle=args.oracle.resolve();observed=oracle.with_name('oracle-observed')
            source=ROOT/'samples/Flexo02.pt3' if args.private else ROOT/'tests/fixtures/integration.pt3'
            raw1=out/'oracle-first.pcm';raw2=out/'oracle-second.pcm';raw3=out/'oracle-observed.pcm';trace=out/'oracle-observed.jsonl'
            for target in (raw1,raw2):run([oracle,source,target],commands)
            run([observed,source,raw3,trace],commands)
            if raw1.read_bytes()!=raw2.read_bytes() or raw1.read_bytes()!=raw3.read_bytes():
                raise ValueError('Independent oracle repeatability/instrumentation PCM conflict')
            reference=out/'oracle-qualified.wav';wrap_raw(raw1,reference)
            if args.private:
                rm,r=pcm.read_wav(ROOT/'samples/Flexo02.wav');cm,c=pcm.read_wav(reference)
                qualification=pcm.compare(r,c,rm,cm)
                if qualification['status']!='BIT_EXACT_PCM':raise ValueError('Pascal adapter fails original direct-export qualification')
                save(out/'oracle-qualification.json',qualification)
            manifest['oracle']={'repeatability':'BIT_EXACT_PCM','instrumentation_pcm':'BIT_EXACT_PCM',
                'executable_sha256':digest(oracle),'observer_executable_sha256':digest(observed),
                'adapter_source_sha256':digest(oracle.with_suffix('.pas')),
                'observer_source_sha256':digest(observed.with_suffix('.pas')),
                'provenance':json.loads((ROOT/'reference/oracle/provenance.json').read_text()),
                'original_pair_qualification':'BIT_EXACT_PCM' if args.private else 'NOT_EXECUTED',
                'adapter_architecture':'Linux x86-64 (separate reference executable)',
                'compiler_version':subprocess.check_output([ROOT/'reference/toolchain/usr/bin/ppcx64','-iV'],text=True).strip() if (ROOT/'reference/toolchain/usr/bin/ppcx64').exists() else 'UNKNOWN'}
            measure('qualified-source-oracle',source,reference,events=trace)
            # Original synthetic integration input supports independently tested
            # AY/YM, clock, timing, rates, preamp and FIR/averager without private music.
            profiles=[('AY-48k',['--ay'],{'ORACLE_AY':'1'},48000),
                ('YM-22k',['--rate','22050'],{'ORACLE_RATE':'22050'},22050),
                ('YM-96k',['--rate','96000'],{'ORACLE_RATE':'96000'},96000),
                ('YM-8k',['--rate','8000'],{'ORACLE_RATE':'8000'},8000),
                ('YM-2MHz',['--clock','2000000'],{'ORACLE_CLOCK':'2000000'},48000),
                ('YM-averager',['--no-filter'],{'ORACLE_NO_FILTER':'1'},48000),
                ('YM-preamp255',['--preamp','255'],{'ORACLE_PREAMP':'255'},48000),
                ('AY-Pentagon',['--ay','--clock','1750000','--interrupt','48.828'],
                 {'ORACLE_AY':'1','ORACLE_CLOCK':'1750000','ORACLE_INTERRUPT':'48.828'},48000)]
            for name,options,settings,rate in profiles:
                env={k:v for k,v in os.environ.items() if not k.startswith('ORACLE_')};env.update(settings)
                run([oracle,ROOT/'tests/fixtures/integration.pt3',raw1],commands,env)
                wrap_raw(raw1,reference,rate)
                measure(name,ROOT/'tests/fixtures/integration.pt3',reference,options,profile={'options':options,'oracle_environment':settings})
            for version,tone_table in ((3,0),(3,2),(5,0),(6,3),(7,1)):
                module=out/f'header-v{version}-table{tone_table}.pt3'
                data=bytearray((ROOT/'tests/fixtures/effects-portamento.pt3').read_bytes())
                data[13]=ord(str(version));data[99]=tone_table;module.write_bytes(data)
                run([observed,module,raw1,trace],commands)
                wrap_raw(raw1,reference)
                measure(module.stem,module,reference,events=trace,
                    profile={'base':manifest['profile_version'],'pt3_version':version,'tone_table':tone_table,
                             'input_provenance':'original synthetic effects-portamento with declared header mutation'})
                shutil.copyfile(module,out/module.stem/'input.pt3')
            for path in (raw1,raw2,raw3,trace,reference):path.unlink()
    except (ValueError,OSError,RuntimeError,ImportError) as e:
        manifest['error']=str(e)
    manifest['commands']=commands
    manifest['results']=[{'fixture':name,'status':r['status'],'report':name+'/report.json'} for name,r in reports]
    passed=bool(reports) and not manifest.get('error') and all(r['status'] in pcm.ACCEPTED for _,r in reports)
    manifest['aggregate_status']=aggregate_result(reports,manifest.get('required_gate',{}),manifest.get('error'))
    save(out/'manifest.json',manifest);(out/'summary.md').write_text(table(reports));page(out,reports)
    checksums={str(f.relative_to(out)):digest(f) for f in sorted(out.rglob('*')) if f.is_file()}
    save(out/'SHA256SUMS.json',checksums)
    archive=out.with_suffix('.tar.gz')
    with tarfile.open(archive,'w:gz') as tar:tar.add(out,arcname=out.name)
    print(f'{manifest["aggregate_status"]}: {len(reports)} fixtures; {out}; {archive}')
    if manifest.get('error'):print(manifest['error'],file=sys.stderr)
    return 0 if passed else 1
if __name__=='__main__':sys.exit(main())
