#!/usr/bin/env python3
"""Linux RSS/PSS and CPU comparison using an isolated real-playback workload."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]

def sample(pid):
    fields = {}
    for line in Path(f'/proc/{pid}/smaps_rollup').read_text().splitlines()[1:]:
        if ':' in line:
            fields[line.split(':', 1)[0]] = int(line.split()[1])
    stat = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
    return {'rss_kib': fields['Rss'], 'pss_kib': fields['Pss'],
            'anonymous_kib': fields['Anonymous'], 'private_dirty_kib': fields['Private_Dirty'],
            'cpu_ticks': int(stat[11]) + int(stat[12])}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=60)
    parser.add_argument('--baseline-binary', type=Path, default=ROOT/'build-memory-baseline/C++Ay')
    parser.add_argument('--binary', type=Path, default=ROOT/'build/C++Ay')
    parser.add_argument('--import-folder', type=Path)
    parser.add_argument('--idle', action='store_true')
    parser.add_argument('--single-track', action='store_true')
    parser.add_argument('--optimized-only', action='store_true')
    parser.add_argument('--report', type=Path, default=ROOT/'evidence/memory-playback.json')
    args = parser.parse_args()
    input_track = ROOT/'samples/Flexo02.pt3'
    if not input_track.exists():
        input_track = ROOT/'tests/fixtures/integration.pt3'
    output = ROOT/'build/memory-investigation'/('idle' if args.idle else 'single-track' if args.single_track else 'playback')
    output.mkdir(parents=True, exist_ok=True)
    jobs = []
    builds = [('optimized-defaults', args.binary)] if args.optimized_only else [('previous-defaults', args.baseline_binary), ('optimized-defaults', args.binary)]
    for name, binary in builds:
        work = output/name
        work.mkdir(exist_ok=True)
        env = os.environ.copy()
        for key in ['QT_QUICK_BACKEND', 'QT_FFMPEG_DECODING_HW_DEVICE_TYPES', 'QT_FFMPEG_ENCODING_HW_DEVICE_TYPES']:
            env.pop(key, None)
        env.update(XDG_CONFIG_HOME=str(work/'config'), XDG_DATA_HOME=str(work/'data'),
                   AYPLAYER_MEMORY_SECONDS=str(args.seconds))
        if args.import_folder:
            env['AYPLAYER_MEMORY_IMPORT'] = str(args.import_folder.resolve())
        if args.idle:
            env['AYPLAYER_MEMORY_IDLE'] = '1'
        if args.single_track:
            env['AYPLAYER_MEMORY_SINGLE_TRACK'] = '1'
        logfile = (work/'run.log').open('w')
        command = [str(binary.resolve()), '--memory-smoke-test',
                   str(input_track), str(ROOT/'tests/fixtures/native-ts.pt3')]
        process = subprocess.Popen(command, cwd=work, env=env, stdout=logfile, stderr=logfile)
        jobs.append({'name':name, 'process':process, 'log':logfile, 'work':work, 'samples':[], 'started':time.monotonic(), 'gpu_libraries':set(), 'binary':binary.resolve()})
    previous_update = 0
    while any(j['process'].poll() is None for j in jobs):
        now = time.monotonic()
        for job in jobs:
            if job['process'].poll() is not None:
                continue
            try:
                item = sample(job['process'].pid)
                item['seconds'] = now-job['started']
                job['samples'].append(item)
            except (FileNotFoundError, ProcessLookupError, PermissionError):
                pass
            if now-job['started'] > args.seconds+30:
                job['process'].kill()
                raise RuntimeError('Playback workload timed out')
        if now-previous_update > 10:
            for job in jobs:
                if job['samples'] and job['process'].poll() is None:
                    latest = job['samples'][-1]
                    try:
                        maps = Path(f"/proc/{job['process'].pid}/maps").read_text()
                        for line in maps.splitlines():
                            if any(token in line.lower() for token in ['libcuda', 'libnvidia', 'libllvm']):
                                job['gpu_libraries'].add(line.split()[-1])
                    except FileNotFoundError:
                        pass
                    print(f"{job['name']}: {latest['seconds']:.1f}s, RSS {latest['rss_kib']/1024:.1f} MiB, PSS {latest['pss_kib']/1024:.1f} MiB", flush=True)
            previous_update = now
        time.sleep(.25)
    results = {}
    for job in jobs:
        job['log'].close()
        samples = job['samples']
        (job['work']/'memory-samples.json').write_text(json.dumps(samples,indent=2)+'\n')
        warm = [s for s in samples if 5 < s['seconds'] < args.seconds-1] or samples
        if not warm:
            raise RuntimeError(f"No process memory samples: {job['name']}")
        last = warm[-1]
        first = warm[0]
        workload_path = job['work']/'evidence/memory-workload.json'
        workload = json.loads(workload_path.read_text()) if workload_path.exists() else {'status':'FAIL', 'log':(job['work']/'run.log').read_text()}
        if job['process'].returncode:
            workload = {'status':'FAIL', 'log':(job['work']/'run.log').read_text()}
        results[job['name']] = {'exit_code':job['process'].returncode, 'workload':workload,
           'peak_rss_mib': max(s['rss_kib'] for s in samples)/1024,
           'peak_pss_mib': max(s['pss_kib'] for s in samples)/1024,
           'steady_rss_mib': last['rss_kib']/1024, 'steady_pss_mib':last['pss_kib']/1024,
           'warm_rss_growth_mib':(last['rss_kib']-first['rss_kib'])/1024,
           'cpu_percent_one_core': (last['cpu_ticks']-first['cpu_ticks'])/os.sysconf('SC_CLK_TCK')/max(.001,last['seconds']-first['seconds'])*100,
           'mapped_gpu_libraries':sorted(job['gpu_libraries']),
           'executable_sha256':hashlib.sha256(job['binary'].read_bytes()).hexdigest(), 'samples':samples}
    report = {'platform':'Linux /proc smaps_rollup, MiB; CPU percent of one logical core',
              'same_source_and_workload':True, 'baseline':'AYPLAYER_MEMORY_BASELINE diagnostic build: original automatic Qt Quick and FFmpeg GPU defaults',
              'cases':results}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2)+'\n')
    for name, result in results.items():
        print(name, {k:v for k,v in result.items() if k!='samples'}, flush=True)
    if any(r['exit_code'] or r['workload']['status']!='PASS' for r in results.values()):
        raise SystemExit(1)

if __name__=='__main__':
    main()
