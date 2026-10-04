#!/usr/bin/env python3
import json,pathlib,subprocess,wave
root=pathlib.Path(__file__).resolve().parents[1];out=root/'evidence/log-corpus';out.mkdir(exist_ok=True)
frames=[]
for shape in range(16):
 frame=[(0,5+shape),(1,0),(2,9),(3,0),(4,0),(5,0),(6,shape),(7,0),(8,16),(9,10),(10,12),(11,1),(12,0),(13,shape),(13,shape)]
 frames.append(frame);frames.extend([[] for _ in range(3)])
header=b'PSG\x1a'+bytes(12)
psg=header+b''.join(b''.join(bytes(pair) for pair in frame)+b'\xff' for frame in frames)
cases={'all-shapes-and-retrigger.psg':psg,'skip.psg':header+bytes([0,2,8,15,7,62,254,3,255,253]),'unknown-registers.psg':header+bytes([0,3,8,15,7,62,15,255,254,1,255])}
planes=[]
for reg in range(14):
 values=[];previous=0
 for frame in frames:
  writes=[v for r,v in frame if r==reg]
  if writes:previous=writes[-1]
  values.append(previous if reg!=13 or writes else 255)
 planes.append(bytes(values))
cases['all-shapes.ym']=b'YM3!'+b''.join(planes)
cases['all-shapes-loop.ym']=b'YM3b'+b''.join(planes)+bytes(4)
results=[]
for name,data in cases.items():
 module=out/name;module.write_bytes(data);pcm=out/(name+'.pcm');trace=out/(name+'.reference.jsonl');candidate=out/(name+'.wav');ctrace=out/(name+'.candidate.jsonl');candidate.unlink(missing_ok=True);ctrace.unlink(missing_ok=True)
 for command in [[root/'reference/oracle/oracle-logs',module,pcm,trace],[root/'build/aytool','render',module,candidate],[root/'build/aytool','trace',module,ctrace]]:
  result=subprocess.run([str(s) for s in command],capture_output=True,text=True)
  if result.returncode:raise RuntimeError(result.stdout+result.stderr)
 with wave.open(str(candidate)) as f:c=f.readframes(f.getnframes())
 r={'case':name,'pcm_exact':c==pcm.read_bytes(),'events_exact':trace.read_bytes()==ctrace.read_bytes(),'pcm_bytes':len(c)};r['status']='PASS' if r['pcm_exact'] and r['events_exact'] else 'FAIL';results.append(r);print(r)
(root/'evidence/log-regression.json').write_text(json.dumps(results,indent=2))
raise SystemExit(any(r['status']!='PASS' for r in results))
