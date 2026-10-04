import importlib.util,pathlib,struct,tempfile,unittest
import numpy as np
spec=importlib.util.spec_from_file_location('compare',pathlib.Path(__file__).resolve().parents[1]/'tools/compare_pcm.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
class ComparatorTests(unittest.TestCase):
 def compare(self,r,c,rate=4):
  r=np.array(r,dtype=np.int64);c=np.array(c,dtype=np.int64)
  if r.ndim==1:r=r[:,None]
  if c.ndim==1:c=c[:,None]
  def meta(a):return {'rate_hz':rate,'channels':a.shape[1],'bits':16,'frames':len(a)}
  return mod.compare(r,c,meta(r),meta(c))
 def test_exact_and_silence(self):
  self.assertEqual(self.compare([0,1,-2,3],[0,1,-2,3])['status'],'BIT_EXACT_PCM')
  self.assertEqual(self.compare([0]*8,[0]*8)['channels'][0]['snr_status'],'perfect_silence')
 def test_calculable_metrics(self):
  m=mod.metrics(np.array([3,4]),np.array([4,2]));self.assertEqual(m['max_abs_error_lsb'],2);self.assertAlmostEqual(m['mean_error_lsb'],-.5);self.assertAlmostEqual(m['relative_rms_error'],np.sqrt(5/25))
 def test_changes_fail(self):
  base=[[100,200],[300,500],[900,1200],[1000,1800]]
  for changed in [np.array(base)*2,np.array(base)+1,np.array(base)[:,::-1],np.roll(base,1,axis=0)]:self.assertEqual(self.compare(base,changed)['status'],'MISMATCH')
 def test_lengths_channels_empty(self):
  self.assertEqual(self.compare([1,2],[1,2,3])['status'],'MISMATCH')
  self.assertEqual(self.compare([1,2],[[1,2],[2,3]])['status'],'MISMATCH')
  self.assertEqual(self.compare([],[])['status'],'MISMATCH')
 def test_noisy_silence_fails(self):self.assertEqual(self.compare([0]*8,[0,0,0,0,0,0,0,1])['status'],'MISMATCH')
 def test_window_gate(self):
  r=np.array([30000]*8+[0]*4);c=r.copy();c[-1]=1;self.assertEqual(self.compare(r,c)['status'],'MISMATCH')
 def test_sparse_near_match(self):
  r=np.full(48000,30000);c=r.copy();c[20]+=1;report=self.compare(r,c,48000);self.assertEqual(report['status'],'NEAR_MATCH_TARGET_MET');self.assertEqual(report['channels'][0]['unequal_samples'],1)
 def test_sparse_above_threshold(self):
  r=np.full(48000,100);c=r.copy();c[20]+=1;self.assertEqual(self.compare(r,c,48000)['status'],'MISMATCH')
 def test_threshold_boundary_and_maximum_guard(self):
  # Exactly one error in four samples: relative error = 1/(2*R).
  for amplitude in (32767,):
   r=np.full(4,amplitude);c=r.copy();c[1]-=1
   self.assertAlmostEqual(self.compare(r,c)['channels'][0]['relative_rms_error'],1/(2*amplitude))
  r=np.full(10000,10000);c=r.copy();c[0]+=1
  self.assertEqual(self.compare(r,c,10000)['status'],'NEAR_MATCH_TARGET_MET')
  r=np.full(9999,10000);c=r.copy();c[0]+=1
  self.assertEqual(self.compare(r,c,9999)['status'],'MISMATCH')
  r=np.full(48000,30000);c=r.copy();c[0]+=2
  self.assertLess(self.compare(r,c,48000)['channels'][0]['relative_rms_error'],1e-6)
  self.assertEqual(self.compare(r,c,48000)['status'],'MISMATCH')
 def test_right_channel_partial_tail_and_locations(self):
  r=np.full((10,2),100);c=r.copy();c[8,1]+=1;c[9,1]+=3
  report=self.compare(r,c)
  self.assertEqual(report['status'],'MISMATCH')
  self.assertEqual(report['channels'][0]['unequal_samples'],0)
  self.assertEqual(report['first_mismatch'],[8,1])
  self.assertEqual(report['first_mismatch_seconds'],2)
  self.assertEqual(report['channels'][1]['largest_error_frame'],9)
  self.assertEqual(report['worst_window']['start_frame'],8)
  self.assertEqual(report['worst_window']['end_frame'],10)
  self.assertAlmostEqual(report['worst_window']['metrics']['rms_error_lsb'],np.sqrt(5))
 def test_insert_drop_and_transition_window(self):
  r=np.arange(20)+100
  for c in (np.insert(r,10,r[10]),np.delete(r,10)):
   self.assertEqual(self.compare(r,c)['reason'],'FORMAT_OR_LENGTH_MISMATCH')
  r=np.full((48000,1),30000);c=r.copy();c[20]+=1
  meta={'rate_hz':48000,'channels':1,'bits':16,'frames':48000}
  report=mod.compare(r,c,meta,meta,[(20,21)])
  self.assertEqual(report['status'],'MISMATCH')
  self.assertEqual(report['worst_window']['kind'],'predetermined_transition')
 def test_invalid_samples_and_full_range_arithmetic(self):
  meta={'rate_hz':4,'channels':1,'bits':16,'frames':1}
  for a in (np.array([[np.nan]]),np.array([[np.inf]]),np.array([[32768]])):
   with self.assertRaises(ValueError):mod.compare(a,a,meta,meta)
  result=self.compare([-32768],[32767])
  self.assertEqual(result['channels'][0]['max_abs_error_lsb'],65535)
  json=__import__('json');json.dumps(result,allow_nan=False)
 def test_residual_keeps_full_range(self):
  with tempfile.TemporaryDirectory() as d:
   path=pathlib.Path(d)/'residual.wav'
   mod.write_difference(path,np.array([[-32768]]),np.array([[32767]]),{'rate_hz':4,'channels':1,'bits':16})
   raw=path.read_bytes();self.assertEqual(struct.unpack_from('<H',raw,20)[0],3)
   self.assertEqual(struct.unpack_from('<f',raw,len(raw)-4)[0],65535/32768)
 def test_ordered_event_conflict_and_invalid_order(self):
  import json
  with tempfile.TemporaryDirectory() as d:
   r=pathlib.Path(d)/'r.jsonl';c=pathlib.Path(d)/'c.jsonl'
   events=[{'tick':0,'ordinal':0,'chip':0,'register':13,'value':9},
           {'tick':0,'ordinal':1,'chip':0,'register':13,'value':9}]
   r.write_text(''.join(json.dumps(e)+'\n' for e in events));c.write_bytes(r.read_bytes())
   self.assertEqual(mod.compare_events(r,c)['status'],'EXACT_ORDERED_EVENTS')
   events[1]['value']=10;c.write_text(''.join(json.dumps(e)+'\n' for e in events))
   self.assertEqual(mod.compare_events(r,c)['first_divergence'],1)
   self.assertEqual(mod.compare_events(r,c)['status'],'FAIL_EVENTS')
   events[1]['ordinal']=0;c.write_text(''.join(json.dumps(e)+'\n' for e in events))
   with self.assertRaises(ValueError):mod.compare_events(r,c)
 def test_aggregate_cannot_pass_one_failure_or_zero_cases(self):
  import sys
  sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
  import release_fidelity
  cases=[('left',{'status':'BIT_EXACT_PCM'}),('right',{'status':'MISMATCH'})]
  self.assertEqual(release_fidelity.aggregate_result(cases,{'status':'NEAR_MATCH_TARGET_MET'}),'MISMATCH')
  self.assertEqual(release_fidelity.aggregate_result([],{'status':'BIT_EXACT_PCM'}),'MISMATCH')
  cases[1][1]['status']='NEAR_MATCH_TARGET_MET'
  self.assertEqual(release_fidelity.aggregate_result(cases,{'status':'NEAR_MATCH_TARGET_MET'}),'PASS')
  self.assertEqual(release_fidelity.aggregate_result(cases,{'status':'BLOCKED_MISSING_REFERENCE'},'missing'),'BLOCKED_MISSING_REFERENCE')
 def test_riff_padding_and_bounds(self):
  fmt=struct.pack('<HHIIHH',1,1,4,8,2,16)
  body=b'WAVE'+b'JUNK'+struct.pack('<I',1)+b'x\0'+b'fmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',4)+struct.pack('<hh',3,-4)
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d)/'a.wav'
   for oversize in (0,):
    p.write_bytes(b'RIFF'+struct.pack('<I',len(body)+oversize)+body);meta,a=mod.read_wav(p);self.assertEqual(a.tolist(),[[3],[-4]]);self.assertEqual(meta['riff_oversize_bytes'],oversize)
   p.write_bytes(b'RIFF'+struct.pack('<I',len(body)+8)+body)
   with self.assertRaises(ValueError):mod.read_wav(p)
   p.write_bytes(b'RIFF'+struct.pack('<I',len(body))+body[:-1])
   with self.assertRaises(ValueError):mod.read_wav(p)
   zero_rate=body.replace(fmt,struct.pack('<HHIIHH',1,1,0,0,2,16))
   p.write_bytes(b'RIFF'+struct.pack('<I',len(zero_rate))+zero_rate)
   with self.assertRaises(ValueError):mod.read_wav(p)
   missing_pad=body+b'JUNK'+struct.pack('<I',1)+b'x'
   p.write_bytes(b'RIFF'+struct.pack('<I',len(missing_pad))+missing_pad)
   with self.assertRaises(ValueError):mod.read_wav(p)
if __name__=='__main__':unittest.main()
