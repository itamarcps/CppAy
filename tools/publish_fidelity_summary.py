#!/usr/bin/env python3
"""Publish small nonprivate numerical evidence; generate/check README table."""
import argparse
import json
import pathlib
import shutil
import sys
import release_fidelity as f
ROOT=f.ROOT
p=argparse.ArgumentParser();p.add_argument('--evidence',type=pathlib.Path,required=True);p.add_argument('--check',action='store_true');a=p.parse_args();out=a.evidence.resolve()
manifest=json.loads((out/'manifest.json').read_text())
if manifest['aggregate_status']!='PASS':p.error('Cannot publish a failing evidence summary')
reports=[(x['fixture'],json.loads((out/x['report']).read_text())) for x in manifest['results']]
selected=[x for name in ('Flexo02','native-ts.pt3','AY-48k','YM-averager') for x in reports if x[0]==name]
table=f.table(selected)
readme=ROOT/'README.md';text=readme.read_text();start='<!-- fidelity-table:start -->';end='<!-- fidelity-table:end -->'
if start not in text or end not in text:p.error('README lacks fidelity table markers')
expected=text.split(start)[0]+start+'\n'+table+end+text.split(end)[1]
if a.check:
    if expected!=text:raise SystemExit('README fidelity table is stale')
    stored=json.loads((ROOT/'docs/fidelity/summary.json').read_text())
    if stored['candidate']['renderer_sha256']!=manifest['candidate']['renderer_sha256'] or stored['results']!=[r for _,r in reports]:
        raise SystemExit('Published report identity/results are stale')
    print('PASS: README table and summary match actual reports');sys.exit(0)
readme.write_text(expected)
folder=ROOT/'docs/fidelity';folder.mkdir(parents=True,exist_ok=True)
candidate=dict(manifest['candidate']);candidate.pop('renderer_path',None)
for key in list(candidate):
    if key.endswith(':PATH') or key.endswith(':FILEPATH'):candidate.pop(key)
summary={'schema_version':2,'measurement_utc':manifest['measurement_utc'],
         'candidate':candidate,'candidate_inputs_sha256':manifest['candidate_inputs_sha256'],
         'required_gate':manifest['required_gate'],'oracle':manifest.get('oracle'),
         'profile_manifest':'../../tests/fixtures/fidelity-manifest.json','results':[r for _,r in reports]}
(folder/'summary.json').write_text(json.dumps(summary,indent=2,allow_nan=False)+'\n')
(folder/'summary.md').write_text('# Measured PCM results\n\n'+f.table(reports)+'\nMeasured '+manifest['measurement_utc']+' against source revision `'+candidate['source_revision']+'`; renderer SHA-256 `'+candidate['renderer_sha256']+'`.\n')
source=out/'native-ts.pt3';shutil.copyfile(source/'comparison.png',ROOT/'docs/images/fidelity.png')
listen=folder/'listening';listen.mkdir(exist_ok=True)
public_report=next(r for name,r in reports if name=='native-ts.pt3')
case=listen/'native-ts.pt3';case.mkdir(exist_ok=True)
for filename in ('reference.wav','candidate.wav','difference.wav','comparison.png','report.json'):
    shutil.copyfile(source/filename,case/filename)
f.save(listen/'manifest.json',{'fixture':'native-ts.pt3','redistribution':manifest['public_corpus']['redistribution'],
    'reference_identity':public_report['reference'],'candidate_identity':candidate,'input':public_report['input'],
    'interval':'entire 30,723-frame track','profile':manifest['profile']})
(listen/'summary.md').write_text(f.table([('native-ts.pt3',public_report)]));f.page(listen,[('native-ts.pt3',public_report)])
print('Published small public example and numerical-only private fixture summary')
