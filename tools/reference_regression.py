#!/usr/bin/env python3
"""Independent Pascal/C++ source regression corpus; originals remain immutable."""
import concurrent.futures,hashlib,json,pathlib,struct,subprocess,wave
root=pathlib.Path(__file__).resolve().parents[1];out=root/'evidence/corpus';out.mkdir(exist_ok=True)
cases=[]
original=(root/'samples/Flexo02.pt3').read_bytes()
for version,table in [(3,0),(3,2),(5,0),(6,3),(7,1)]:
 data=bytearray(original);data[13]=ord(str(version));data[99]=table;data[98]=32
 name=f'fixture-header-v{version}-table{table}';(out/(name+'.pt3')).write_bytes(data);cases.append(name)
# Synthetic patterns isolate commands and all 16 envelope shapes. Music is generated,
# but expected PCM and events come exclusively from the unchanged Pascal routines.
def synthetic(name,rows,version=7,sample=(0,15,0,0)):
 data=bytearray(210);data[:len(b'ProTracker 3.7')]=b'ProTracker 3.7';data[13]=ord(str(version));data[30:30+len(name)]=name.encode();data[98]=32;data[99]=0;data[100]=3;data[101]=1;data[102]=0;data[103:105]=struct.pack('<H',204);data[201]=0;data[202]=255
 streams=[bytes([0xb1,1,0xd1,0xcf])+b''.join(rows)+b'\0',bytes([0xb1,1,0xd1,0xcf])+bytes([0x60])*len(rows)+b'\0',bytes([0xb1,1,0xd1,0xcf])+bytes([0x70])*len(rows)+b'\0']
 for i,stream in enumerate(streams):struct.pack_into('<H',data,204+i*2,len(data));data.extend(stream)
 ptr=len(data);struct.pack_into('<H',data,107,ptr);data.extend(bytes([0,1])+bytes(sample))
 ptr=len(data);struct.pack_into('<H',data,169,ptr);data.extend(bytes([0,1,0]))
 (out/(name+'.pt3')).write_bytes(data);cases.append(name)
for shape in range(1,16):synthetic(f'envelope-{shape}',[bytes([0x10+shape,0,2,2,0x50]),bytes([0xd0])]*4)
synthetic('envelope-disabled-reset',[bytes([0x10,2,0x50]),bytes([0xd0])]*4)
# shape 0 uses B0 off; source B2..BF provides 1..14; 1F provides shape15.
synthetic('effects-gliss',[bytes([1,0x50,0,1,0]),bytes([0xd0])]*8)
synthetic('effects-portamento',[bytes([0x50]),bytes([2,0x60,1,0,0,1,0])]+[bytes([0xd0])]*12)
synthetic('effects-vibrato',[bytes([5,0x50,2,3])]+[bytes([0xd0])]*12)
synthetic('effects-env-slide',[bytes([8,0xb9,0,10,0x50,1,1,0])]+[bytes([0xd0])]*12)
synthetic('effects-tempo',[bytes([9,0x50,1]),bytes([9,0x60,2]),bytes([9,0x70,3])]*4)
synthetic('noise-slide',[bytes([0x2a,0x50])]+[bytes([0xd0])]*12,sample=(6,0x2f,1,0))
def run(name):
 module=out/(name+'.pt3');expected=out/(name+'.pcm');candidate=out/(name+'.wav');trace=out/(name+'.reference.jsonl');actualtrace=out/(name+'.candidate.jsonl')
 for p in (candidate,actualtrace):p.unlink(missing_ok=True)
 commands=[[str(root/'reference/oracle/oracle-observed'),str(module),str(expected),str(trace)],[str(root/'build/aytool'),'render',str(module),str(candidate)],[str(root/'build/aytool'),'trace',str(module),str(actualtrace)]]
 for command in commands:
  r=subprocess.run(command,capture_output=True,text=True)
  if r.returncode:return {'case':name,'status':'FAIL_EXECUTION','command':command,'error':r.stdout+r.stderr}
 with wave.open(str(candidate)) as w:pcm=w.readframes(w.getnframes())
 result={'case':name,'input_sha256':hashlib.sha256(module.read_bytes()).hexdigest(),'pcm_bytes':len(pcm),'pcm_exact':pcm==expected.read_bytes(),'events_exact':trace.read_bytes()==actualtrace.read_bytes(),'source':'synthetic' if not name.startswith('fixture') else 'original fixture with declared version/table header mutation'}
 result['status']='PASS' if result['pcm_exact'] and result['events_exact'] else 'FAIL';return result
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(run,cases))
(root/'evidence/reference-regression.json').write_text(json.dumps(results,indent=2))
for r in results:print(r['case'],r['status'],r.get('error',''))
raise SystemExit(any(r['status']!='PASS' for r in results))
