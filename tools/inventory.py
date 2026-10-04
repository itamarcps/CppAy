#!/usr/bin/env python3
"""Preserve archives; reject traversal/links before extracting developer references."""
import hashlib,json,pathlib,subprocess,zipfile,wave
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'reference/extracted';out.mkdir(parents=True,exist_ok=True)
items=[]
for p in sorted((root/'docs').iterdir()):
 if p.suffix.lower() not in ('.zip','.7z','.rar'):continue
 entry={'path':str(p.relative_to(root)),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
 try:
  if p.suffix=='.zip':
   with zipfile.ZipFile(p) as z:
    names=z.namelist()
    if any((i.external_attr>>16)&0o170000==0o120000 for i in z.infolist()):raise ValueError('symlink')
  else:
   listing=subprocess.run(['7z','l','-slt',str(p)],capture_output=True,text=True,check=True).stdout.split('----------',1)[1]
   names=[line[7:] for line in listing.splitlines() if line.startswith('Path = ')]
   if 'Symbolic Link =' in listing or 'Hard Link =' in listing:raise ValueError('link')
  for name in names:
   q=pathlib.PurePosixPath(name.replace('\\','/'))
   if q.is_absolute() or '..' in q.parts or ':' in name:raise ValueError('unsafe member: '+name)
  dest=out/p.stem;dest.mkdir(exist_ok=True)
  args=['unrar','x','-o+',str(p),str(dest)+'/'] if p.suffix=='.rar' else ['7z','x','-y','-o'+str(dest),str(p)]
  result=subprocess.run(args,capture_output=True,text=True)
  entry.update(members=len(names),extracted=result.returncode==0,diagnostic=result.stdout[-700:]+result.stderr[-700:] if result.returncode else '')
 except Exception as e:entry['error']=str(e)
 items.append(entry)
manifest={'archives':items,'reference':{'identity':'AY_Emul 3.0 beta source, December 2024','executable':'unknown','capture':'direct WAV conversion, user confirmed','profile_status':'inferred source defaults'},'fixtures':[]}
for p in sorted((root/'samples').iterdir()):
 e={'path':str(p.relative_to(root)),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
 if p.suffix=='.wav':
  with wave.open(str(p)) as w:e['pcm']={'rate_hz':w.getframerate(),'channels':w.getnchannels(),'bits':w.getsampwidth()*8,'frames':w.getnframes()}
 manifest['fixtures'].append(e)
(root/'evidence/manifest.json').write_text(json.dumps(manifest,indent=2))
print(json.dumps([{'path':i['path'],'extracted':i.get('extracted',False),'error':i.get('error')} for i in items],indent=2))
