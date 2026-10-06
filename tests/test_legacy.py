"""Independent PT2/STC PCM/events plus real CLI PT3 conversion and error cases."""
import hashlib,json,pathlib,subprocess,sys,tempfile,wave,struct
root=pathlib.Path(__file__).resolve().parents[1];tool=pathlib.Path(sys.argv[1]).resolve();fixtures=root/'tests/fixtures/legacy'
def run(*args,ok=True):
 r=subprocess.run([str(tool),*map(str,args)],capture_output=True,text=True)
 assert (r.returncode==0)==ok,(r.returncode,r.stdout,r.stderr)
 return r
with tempfile.TemporaryDirectory(prefix='cppay-legacy-') as temp:
 out=pathlib.Path(temp)
 for f in json.loads((fixtures/'manifest.json').read_text())['fixtures']:
  for name,sha in f['sha256'].items():assert hashlib.sha256((fixtures/name).read_bytes()).hexdigest()==sha
  source=fixtures/f['input'];wav=out/(f['input']+'.wav');run('render',source,wav)
  with wave.open(str(wav)) as w:pcm=w.readframes(w.getnframes())
  assert pcm==(fixtures/f['pcm']).read_bytes(),'Independent PCM mismatch: '+f['input']
  trace=out/(f['input']+'.jsonl');run('trace',source,trace)
  events=lambda path:[json.loads(line) for line in path.read_text().splitlines()]
  assert events(trace)==events(fixtures/f['events']),'Independent ordered events mismatch: '+f['input']
  converted=out/(f['input']+'.pt3');run('convert-pt3',source,converted)
  other=out/(f['input']+'-converted.wav');run('render',converted,other)
  with wave.open(str(other)) as w:assert w.readframes(w.getnframes())==pcm,'PT3 PCM differs'
  repeat=out/(f['input']+'-repeat.pt3');run('convert-pt3',source,repeat);assert repeat.read_bytes()==converted.read_bytes()
  before=converted.read_bytes();run('convert-pt3',source,converted,ok=False);assert converted.read_bytes()==before
  bad=out/('bad.'+source.suffix[1:]);bad.write_bytes(source.read_bytes()[:20]);badout=out/'bad.pt3'
  run('convert-pt3',bad,badout,ok=False);assert not badout.exists();run('render',bad,out/'bad.wav',ok=False)
  # Mixed-case filenames and no-filter/AY profiles.
  mixed=out/('music.'+source.suffix[1:].title());mixed.write_bytes(source.read_bytes());run('inspect',mixed)
  for options in (['--ay'],['--no-filter'],['--rate','22050']):
   a=out/'profile-source.wav';b=out/'profile-converted.wav';run('render',source,a,*options);run('render',converted,b,*options)
   with wave.open(str(a)) as w:x=w.readframes(w.getnframes())
   with wave.open(str(b)) as w:assert w.readframes(w.getnframes())==x
   a.unlink();b.unlink()
 # Valid STC with 256 positions: conversion must respect PT3's 255 limit.
 data=bytearray((fixtures/'loops.stc').read_bytes());pointer=len(data)
 data[1:3]=struct.pack('<H',pointer);data+=bytes([255])+bytes([0,0])*256
 data[25:27]=struct.pack('<H',len(data));large=out/'too-many.stc';large.write_bytes(data)
 destination=out/'too-many.pt3';r=run('convert-pt3',large,destination,ok=False)
 assert 'pattern/position limits' in r.stderr and not destination.exists()
 print('PASS: independent PT2/STC PCM/events, conversion, profiles, malformed input and no overwrite')
