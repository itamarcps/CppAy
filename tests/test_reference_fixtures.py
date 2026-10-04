import hashlib,importlib.util,json,pathlib,subprocess,sys,tempfile
root=pathlib.Path(__file__).resolve().parents[1];folder=root/'tests/fixtures'
spec=importlib.util.spec_from_file_location('compare',root/'tools/compare_pcm.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
manifest=json.loads((folder/'manifest.json').read_text())
with tempfile.TemporaryDirectory() as work:
 work=pathlib.Path(work)
 for case in manifest['cases']:
  name=case['input']
  for file,digest in case['files'].items():
   if hashlib.sha256((folder/file).read_bytes()).hexdigest()!=digest:raise AssertionError('Immutable fixture hash mismatch: '+file)
  candidate=work/(name+'.wav');trace=work/(name+'.jsonl')
  subprocess.run([sys.argv[1],'render',str(folder/name),str(candidate)],check=True,capture_output=True)
  subprocess.run([sys.argv[1],'trace',str(folder/name),str(trace)],check=True,capture_output=True)
  meta,pcm=mod.read_wav(candidate)
  if pcm.astype('<i2').tobytes()!=(folder/(name+'.pcm')).read_bytes():raise AssertionError('Independent PCM mismatch: '+name)
  if mod.compare_events(folder/(name+'.reference.jsonl'),trace)['status']!='EXACT_ORDERED_EVENTS':raise AssertionError('Independent event mismatch: '+name)
  print(name,'BIT_EXACT_PCM + EXACT_ORDERED_EVENTS')
 # File timing must agree with an explicit override, and changing the override
 # must alter event scheduling. These are API checks, not regenerated goldens.
 timed=bytearray((folder/'skip.psg').read_bytes());timed[4]=10;timed[5]=60
 source=work/'timed.psg';source.write_bytes(timed)
 for label,args in [('auto',[]),('explicit',['--interrupt','60']),('override',['--interrupt','50'])]:
  subprocess.run([sys.argv[1],'render',str(source),str(work/(label+'.wav')),*args],check=True,capture_output=True)
 _,automatic=mod.read_wav(work/'auto.wav');_,explicit=mod.read_wav(work/'explicit.wav');_,override=mod.read_wav(work/'override.wav')
 if automatic.tobytes()!=explicit.tobytes() or len(automatic)==len(override):raise AssertionError('PSG timing precedence failed')
 timed[4]=11;source.write_bytes(timed)
 rejected=subprocess.run([sys.argv[1],'render',str(source),str(work/'invalid.wav')],capture_output=True)
 if rejected.returncode==0 or (work/'invalid.wav').exists():raise AssertionError('Unsupported PSG version accepted')
 print('PSG header timing, explicit override and version bounds passed')
