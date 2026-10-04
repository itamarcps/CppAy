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
  for changed in [np.array(base)*2,np.array(base)+1,np.array(base)[:,::-1],np.roll(base,1,axis=0)]:self.assertEqual(self.compare(base,changed)['status'],'FAIL_PCM')
 def test_lengths_channels_empty(self):
  self.assertEqual(self.compare([1,2],[1,2,3])['status'],'FAIL_METADATA')
  self.assertEqual(self.compare([1,2],[[1,2],[2,3]])['status'],'FAIL_METADATA')
  self.assertEqual(self.compare([],[])['status'],'FAIL_EMPTY')
 def test_noisy_silence_fails(self):self.assertEqual(self.compare([0]*8,[0,0,0,0,0,0,0,1])['status'],'FAIL_PCM')
 def test_window_gate(self):
  r=np.array([30000]*8+[0]*4);c=r.copy();c[-1]=1;self.assertEqual(self.compare(r,c)['status'],'FAIL_PCM')
 def test_sparse_near_match(self):
  r=np.full(48000,30000);c=r.copy();c[20]+=1;report=self.compare(r,c,48000);self.assertEqual(report['status'],'NEAR_MATCH_TARGET_MET');self.assertEqual(report['channels'][0]['unequal_samples'],1)
 def test_sparse_above_threshold(self):
  r=np.full(48000,100);c=r.copy();c[20]+=1;self.assertEqual(self.compare(r,c,48000)['status'],'FAIL_PCM')
 def test_riff_padding_and_bounds(self):
  fmt=struct.pack('<HHIIHH',1,1,4,8,2,16)
  body=b'WAVE'+b'JUNK'+struct.pack('<I',1)+b'x\0'+b'fmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',4)+struct.pack('<hh',3,-4)
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d)/'a.wav'
   for oversize in (0,8):
    p.write_bytes(b'RIFF'+struct.pack('<I',len(body)+oversize)+body);meta,a=mod.read_wav(p);self.assertEqual(a.tolist(),[[3],[-4]]);self.assertEqual(meta['riff_oversize_bytes'],oversize)
   p.write_bytes(b'RIFF'+struct.pack('<I',len(body))+body[:-1])
   with self.assertRaises(ValueError):mod.read_wav(p)
   zero_rate=body.replace(fmt,struct.pack('<HHIIHH',1,1,0,0,2,16))
   p.write_bytes(b'RIFF'+struct.pack('<I',len(zero_rate))+zero_rate)
   with self.assertRaises(ValueError):mod.read_wav(p)
   missing_pad=body+b'JUNK'+struct.pack('<I',1)+b'x'
   p.write_bytes(b'RIFF'+struct.pack('<I',len(missing_pad))+missing_pad)
   with self.assertRaises(ValueError):mod.read_wav(p)
if __name__=='__main__':unittest.main()
