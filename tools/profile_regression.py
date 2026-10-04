#!/usr/bin/env python3
import concurrent.futures,hashlib,json,os,pathlib,subprocess,wave
root=pathlib.Path(__file__).resolve().parents[1];out=root/'evidence/profiles';out.mkdir(exist_ok=True)
cases=[('AY-48k',['--ay'],{'ORACLE_AY':'1'}),('YM-22k',['--rate','22050'],{'ORACLE_RATE':'22050'}),('YM-96k',['--rate','96000'],{'ORACLE_RATE':'96000'}),('YM-8k',['--rate','8000'],{'ORACLE_RATE':'8000'}),('YM-clock-2M',['--clock','2000000'],{'ORACLE_CLOCK':'2000000'}),('YM-averager',['--no-filter'],{'ORACLE_NO_FILTER':'1'}),('YM-preamp255',['--preamp','255'],{'ORACLE_PREAMP':'255'}),('AY-Pentagon',['--ay','--clock','1750000','--interrupt','48.828'],{'ORACLE_AY':'1','ORACLE_CLOCK':'1750000','ORACLE_INTERRUPT':'48.828'})]
def run(case):
 name,options,settings=case;expected=out/(name+'.pcm');candidate=out/(name+'.wav');candidate.unlink(missing_ok=True)
 env=os.environ.copy();env.update(settings)
 a=subprocess.run([str(root/'reference/oracle/oracle'),str(root/'samples/Flexo02.pt3'),str(expected)],env=env,capture_output=True,text=True)
 b=subprocess.run([str(root/'build/aytool'),'render',str(root/'samples/Flexo02.pt3'),str(candidate)]+options,capture_output=True,text=True)
 if a.returncode or b.returncode:return {'profile':name,'status':'FAIL_EXECUTION','error':a.stdout+a.stderr+b.stdout+b.stderr}
 with wave.open(str(candidate)) as w:pcm=w.readframes(w.getnframes());rate=w.getframerate()
 return {'profile':name,'rate_hz':rate,'options':options,'oracle_environment':settings,'pcm_bytes':len(pcm),'status':'BIT_EXACT_PCM' if pcm==expected.read_bytes() else 'FAIL_PCM','candidate_pcm_sha256':hashlib.sha256(pcm).hexdigest(),'oracle_pcm_sha256':hashlib.sha256(expected.read_bytes()).hexdigest()}
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(run,cases))
(root/'evidence/profile-regression.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
raise SystemExit(any(r['status']!='BIT_EXACT_PCM' for r in results))
