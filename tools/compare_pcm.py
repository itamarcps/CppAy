#!/usr/bin/env python3
"""Strict PCM gate: no alignment/gain fitting, resampling or prefix acceptance."""
import argparse,hashlib,json,math,pathlib,struct,sys,wave
import numpy as np

def read_wav(path):
 raw=pathlib.Path(path).read_bytes()
 if len(raw)<12 or raw[:4]!=b'RIFF' or raw[8:12]!=b'WAVE':raise ValueError('not RIFF/WAVE')
 declared=struct.unpack_from('<I',raw,4)[0]+8
 if declared not in (len(raw),len(raw)+8):raise ValueError('invalid RIFF bounds')
 pos=12;fmt=None;data=None
 while pos<len(raw):
  if pos+8>len(raw):raise ValueError('truncated chunk header')
  name=raw[pos:pos+4];n=struct.unpack_from('<I',raw,pos+4)[0];pos+=8
  if pos+n>len(raw):raise ValueError('truncated chunk payload')
  if n&1 and pos+n>=len(raw):raise ValueError('missing chunk padding')
  if name==b'fmt ':
   if fmt is not None or n<16:raise ValueError('invalid/duplicate fmt')
   fmt=struct.unpack_from('<HHIIHH',raw,pos)
  if name==b'data':
   if data is not None:raise ValueError('multiple data chunks')
   data=raw[pos:pos+n]
  pos+=n+(n&1)
 if fmt is None or data is None:raise ValueError('missing fmt/data')
 code,ch,rate,byte_rate,block,bits=fmt
 if code!=1 or ch<1 or rate<1 or bits not in (8,16,24,32) or block!=ch*(bits//8) or byte_rate!=rate*block or len(data)%block:raise ValueError('invalid/unsupported PCM')
 if bits==8:a=np.frombuffer(data,np.uint8).astype(np.int64)-128
 elif bits==24:
  b=np.frombuffer(data,np.uint8).reshape(-1,3).astype(np.int64);a=b[:,0]|(b[:,1]<<8)|(b[:,2]<<16);a=np.where(a&0x800000,a-0x1000000,a)
 else:a=np.frombuffer(data,dtype='<i'+str(bits//8)).astype(np.int64)
 return {'rate_hz':rate,'channels':ch,'bits':bits,'frames':len(a)//ch,'sha256':hashlib.sha256(raw).hexdigest(),'riff_oversize_bytes':declared-len(raw)},a.reshape(-1,ch)

def metrics(r,c):
 e=c.astype(np.float64)-r.astype(np.float64);energy=float(np.sum(r.astype(np.float64)**2));err=float(np.sum(e*e));silent=energy==0
 relative=None if silent else math.sqrt(err/energy)
 snr=None if err==0 or silent else 10*math.log10(energy/err)
 return {'samples':len(r),'unequal_samples':int(np.count_nonzero(e)), 'max_abs_error_lsb':int(np.max(np.abs(e))) if len(e) else None,'mean_error_lsb':float(np.mean(e)) if len(e) else None,'rms_error_lsb':math.sqrt(err/len(e)) if len(e) else None,'relative_rms_error':relative,'residual_snr_db':snr,'snr_status':'perfect_silence' if silent and err==0 else 'silent_reference_with_noise' if silent else 'positive_infinity' if err==0 else 'finite','target_met':bool(len(e) and (err==0 if silent else relative<=1e-6) and np.max(np.abs(e))<=1)}

def compare(r,c,rm,cm):
 compatible=all(rm[k]==cm[k] for k in ('rate_hz','channels','bits','frames'))
 if not compatible:return {'status':'FAIL_METADATA','reference':rm,'candidate':cm,'coverage':'no acceptance on common prefix'}
 if not len(r):return {'status':'FAIL_EMPTY'}
 channels=[];windows=[]
 for ch in range(r.shape[1]):
  m=metrics(r[:,ch],c[:,ch]);e=c[:,ch]-r[:,ch];idx=np.flatnonzero(e)
  m.update(channel=ch,first_mismatch_frame=int(idx[0]) if len(idx) else None,largest_error_frame=int(np.argmax(np.abs(e))),reference_peak_lsb=int(np.max(np.abs(r[:,ch]))),candidate_peak_lsb=int(np.max(np.abs(c[:,ch]))),reference_clipping_samples=int(np.count_nonzero((r[:,ch]==-(1<<(rm['bits']-1)))|(r[:,ch]==(1<<(rm['bits']-1))-1))),candidate_clipping_samples=int(np.count_nonzero((c[:,ch]==-(1<<(rm['bits']-1)))|(c[:,ch]==(1<<(rm['bits']-1))-1))))
  channels.append(m)
 for start in range(0,len(r),rm['rate_hz']):
  end=min(len(r),start+rm['rate_hz']);windows.append({'start_frame':start,'end_frame':end,'channels':[metrics(r[start:end,ch],c[start:end,ch]) for ch in range(r.shape[1])]})
 exact=all(x['unequal_samples']==0 for x in channels)
 near=all(x['target_met'] for x in channels) and all(m['target_met'] for w in windows for m in w['channels'])
 mismatches=np.argwhere(r!=c)
 worst=max(((m['relative_rms_error'] or (float('inf') if m['snr_status']=='silent_reference_with_noise' else 0),w['start_frame'],ch) for w in windows for ch,m in enumerate(w['channels'])),default=(0,0,0))
 return {'status':'BIT_EXACT_PCM' if exact else 'NEAR_MATCH_TARGET_MET' if near else 'FAIL_PCM','reference':rm,'candidate':cm,'coverage_frames':len(r),'excluded_frames':0,'offset_frames':0,'unequal_sample_fraction':sum(x['unequal_samples'] for x in channels)/r.size,'first_mismatch':mismatches[0].tolist() if len(mismatches) else None,'channels':channels,'windows':windows,'worst_window':{'start_frame':worst[1],'channel':worst[2]},'alignment_diagnostics':{'method':'zero offset, equality at start/middle/end; no fitted alignment','segments':[{'start_frame':i,'unequal_samples':int(np.count_nonzero(r[i:i+rm['rate_hz']]!=c[i:i+rm['rate_hz']]))} for i in (0,len(r)//2,max(0,len(r)-rm['rate_hz']))]},'event_trace_status':'UNAVAILABLE_INDEPENDENT_TRACE'}

def compare_events(reference,candidate):
 def read(path):
  def reject(value):raise ValueError('Invalid trace number: '+value)
  events=[json.loads(line,parse_constant=reject) for line in pathlib.Path(path).read_text().splitlines() if line.strip()]
  for event in events:
   for key in ('tick','ordinal','chip','register','value'):
    if type(event.get(key)) is not int or event[key]<0:raise ValueError('Invalid event field '+key)
  return events
 r=read(reference);c=read(candidate)
 first=next((i for i,(a,b) in enumerate(zip(r,c)) if a!=b),None)
 if first is None and len(r)!=len(c):first=min(len(r),len(c))
 return {'status':'EXACT_ORDERED_EVENTS' if first is None else 'FAIL_EVENTS','reference_events':len(r),'candidate_events':len(c),'first_divergence':first,'reference_sha256':hashlib.sha256(pathlib.Path(reference).read_bytes()).hexdigest(),'candidate_sha256':hashlib.sha256(pathlib.Path(candidate).read_bytes()).hexdigest()}

def main():
 p=argparse.ArgumentParser();p.add_argument('reference');p.add_argument('candidate');p.add_argument('--report',required=True);p.add_argument('--difference');p.add_argument('--reference-events');p.add_argument('--candidate-events');a=p.parse_args()
 rm,r=read_wav(a.reference);cm,c=read_wav(a.candidate);report=compare(r,c,rm,cm);report['command']=sys.argv
 if bool(a.reference_events)!=bool(a.candidate_events):raise ValueError('Both event traces are required')
 if a.reference_events:
  report['events']=compare_events(a.reference_events,a.candidate_events);report['event_trace_status']=report['events']['status']
  if report['events']['status']=='FAIL_EVENTS':report['pcm_status']=report['status'];report['status']='FAIL_EVENTS'
 pathlib.Path(a.report).write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
 if a.difference and r.shape==c.shape:
  diff=c-r
  if np.max(np.abs(diff))>32767:raise ValueError('difference exceeds 16-bit: use report metrics, not clipped residual')
  with wave.open(a.difference,'wb') as w:w.setparams((rm['channels'],2,rm['rate_hz'],0,'NONE','not compressed'));w.writeframes(diff.astype('<i2').tobytes())
 print(report['status']);return 0 if report['status'] in ('BIT_EXACT_PCM','NEAR_MATCH_TARGET_MET') else 1
if __name__=='__main__':sys.exit(main())
