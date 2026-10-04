#!/usr/bin/env python3
"""Complete-interval integer PCM gate. Never fit, align, resample or normalize."""
import argparse
import hashlib
import json
import math
import pathlib
import struct
import sys
import numpy as np

SCHEMA_VERSION = 2
ACCEPTED = ('BIT_EXACT_PCM', 'NEAR_MATCH_TARGET_MET')
# Verified original AY_Emul container has an oversized RIFF header, not missing
# audio. Exception applies only to these immutable bytes; all other WAVs strict.
RIFF_QUIRKS = {'ffb13234c83718d28816e954c227eaec2ac043ad0b614652a0c5330f5989f855': 8}

def read_wav(path):
    raw = pathlib.Path(path).read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if len(raw) < 12 or raw[:4] != b'RIFF' or raw[8:12] != b'WAVE':
        raise ValueError('not RIFF/WAVE')
    oversize = struct.unpack_from('<I', raw, 4)[0] + 8 - len(raw)
    if oversize != 0 and RIFF_QUIRKS.get(digest) != oversize:
        raise ValueError('invalid RIFF bounds (unqualified container quirk)')
    pos, fmt, data = 12, None, None
    while pos < len(raw):
        if pos + 8 > len(raw):
            raise ValueError('truncated chunk header')
        name, n = raw[pos:pos+4], struct.unpack_from('<I', raw, pos+4)[0]
        pos += 8
        if pos + n + (n & 1) > len(raw):
            raise ValueError('truncated chunk payload/padding')
        if name == b'fmt ':
            if fmt is not None or n < 16:
                raise ValueError('invalid/duplicate fmt')
            fmt = struct.unpack_from('<HHIIHH', raw, pos)
        if name == b'data':
            if data is not None:
                raise ValueError('multiple data chunks')
            data = raw[pos:pos+n]
        pos += n + (n & 1)
    if fmt is None or data is None:
        raise ValueError('missing fmt/data')
    code, ch, rate, byte_rate, block, bits = fmt
    if (code != 1 or ch < 1 or rate < 1 or bits not in (8, 16, 24, 32)
            or block != ch * (bits // 8) or byte_rate != rate * block
            or len(data) % block):
        raise ValueError('invalid/unsupported integer PCM')
    if bits == 8:
        a = np.frombuffer(data, np.uint8).astype(np.int64) - 128
    elif bits == 24:
        b = np.frombuffer(data, np.uint8).reshape(-1, 3).astype(np.int64)
        a = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        a = np.where(a & 0x800000, a - 0x1000000, a)
    else:
        a = np.frombuffer(data, dtype='<i' + str(bits // 8)).astype(np.int64)
    return dict(rate_hz=rate, channels=ch, bits=bits, frames=len(a)//ch,
                encoding='signed_integer' if bits > 8 else 'unsigned_integer',
                channel_order='L,R' if ch == 2 else 'file_order', sha256=digest,
                riff_oversize_bytes=oversize), a.reshape(-1, ch)

def metrics(r, c):
    # Input range is checked by compare; float64 differences exactly represent
    # every supported integer PCM step, including a full-range 32-bit difference.
    r, c = np.asarray(r), np.asarray(c)
    if r.shape != c.shape or not np.all(np.isfinite(r)) or not np.all(np.isfinite(c)):
        raise ValueError('invalid metric samples')
    e = c.astype(np.float64) - r.astype(np.float64)
    energy = float(np.sum(r.astype(np.float64)**2))
    err = float(np.sum(e*e))
    silent = energy == 0
    relative = None if silent else math.sqrt(err/energy)
    snr = None if err == 0 or silent else 10*math.log10(energy/err)
    unequal = int(np.count_nonzero(e))
    maximum = int(np.max(np.abs(e))) if len(e) else None
    return dict(samples=len(r), unequal_samples=unequal,
                unequal_sample_fraction=unequal/len(r) if len(r) else None,
                max_abs_error_lsb=maximum,
                mean_error_lsb=float(np.mean(e)) if len(e) else None,
                rms_error_lsb=math.sqrt(err/len(e)) if len(e) else None,
                relative_rms_error=relative, residual_snr_db=snr,
                relative_status='not_applicable_zero_reference' if silent else 'finite',
                snr_status='perfect_silence' if silent and err == 0 else
                'silent_reference_with_noise' if silent else
                'positive_infinity' if err == 0 else 'finite',
                target_met=bool(len(e) and (err == 0 if silent else relative <= 1e-6)
                                and maximum <= 1))

def compare(r, c, rm, cm, transition_windows=()):
    report = dict(schema_version=SCHEMA_VERSION, reference=rm, candidate=cm,
                  event_trace_status='UNAVAILABLE_INDEPENDENT_TRACE',
                  offset_frames=0, excluded_frames=0)
    for a, m in ((r, rm), (c, cm)):
        if (a.ndim != 2 or a.shape != (m['frames'], m['channels'])
                or m['rate_hz'] < 1 or m['bits'] not in (8, 16, 24, 32)
                or not np.issubdtype(a.dtype, np.integer)
                or np.any(a < -(1 << (m['bits']-1)))
                or np.any(a > (1 << (m['bits']-1))-1)):
            raise ValueError('invalid PCM samples, shape or metadata')
    if not all(rm[k] == cm[k] for k in ('rate_hz', 'channels', 'bits', 'frames')):
        return dict(report, status='MISMATCH', reason='FORMAT_OR_LENGTH_MISMATCH',
                    coverage_frames=0, coverage='no acceptance on common prefix')
    if not len(r):
        return dict(report, status='MISMATCH', reason='EMPTY_COMPARISON', coverage_frames=0)
    r, c = r.astype(np.int64), c.astype(np.int64)
    rate = rm['rate_hz']
    channels = []
    for ch in range(r.shape[1]):
        m = metrics(r[:, ch], c[:, ch])
        e = c[:, ch] - r[:, ch]
        unequal = np.flatnonzero(e)
        first = int(unequal[0]) if len(unequal) else None
        largest = int(np.argmax(np.abs(e))) if len(unequal) else None
        m.update(channel=ch, first_mismatch_frame=first,
                 first_mismatch_seconds=first/rate if first is not None else None,
                 largest_error_frame=largest,
                 largest_error_seconds=largest/rate if largest is not None else None,
                 location_status='mismatch' if len(unequal) else 'exact_no_error_location',
                 reference_peak_lsb=int(np.max(np.abs(r[:, ch]))),
                 candidate_peak_lsb=int(np.max(np.abs(c[:, ch]))))
        for label, a in (('reference', r), ('candidate', c)):
            m[label+'_clipping_samples'] = int(np.count_nonzero(
                (a[:, ch] == -(1 << (rm['bits']-1))) |
                (a[:, ch] == (1 << (rm['bits']-1))-1)))
        channels.append(m)
    intervals = [(start, min(len(r), start+rate), 'consecutive_1s')
                 for start in range(0, len(r), rate)]
    for start, end in transition_windows:
        if not (0 <= start < end <= len(r)):
            raise ValueError('transition window outside intended interval')
        intervals.append((start, end, 'predetermined_transition'))
    windows = [dict(start_frame=start, end_frame=end, kind=kind,
                    channels=[metrics(r[start:end, ch], c[start:end, ch])
                              for ch in range(r.shape[1])])
               for start, end, kind in intervals]
    exact = all(m['unequal_samples'] == 0 for m in channels)
    near = all(m['target_met'] for m in channels) and all(
        m['target_met'] for w in windows for m in w['channels'])
    def severity(item):
        w, ch, m = item
        return (m['snr_status'] == 'silent_reference_with_noise',
                m['relative_rms_error'] or 0, m['max_abs_error_lsb'])
    worst, worst_ch, worst_metrics = max(
        ((w, ch, m) for w in windows for ch, m in enumerate(w['channels'])), key=severity)
    firsts = [(m['first_mismatch_frame'], ch) for ch, m in enumerate(channels)
              if m['first_mismatch_frame'] is not None]
    first = min(firsts) if firsts else None
    count = sum(m['unequal_samples'] for m in channels)
    report.update(status='BIT_EXACT_PCM' if exact else 'NEAR_MATCH_TARGET_MET' if near else 'MISMATCH',
                  reason='COMPLETE_INTERVAL_EQUAL' if exact else 'PCM_TARGET_MET' if near else 'PCM_TARGET_NOT_MET',
                  coverage_frames=len(r), duration_seconds=len(r)/rate,
                  unequal_samples=count, unequal_sample_fraction=count/r.size,
                  first_mismatch=list(first) if first else None,
                  first_mismatch_seconds=first[0]/rate if first else None,
                  channels=channels, windows=windows,
                  worst_window=dict(start_frame=worst['start_frame'], end_frame=worst['end_frame'],
                                    kind=worst['kind'], channel=worst_ch, metrics=worst_metrics),
                  alignment_diagnostics=dict(method='zero-offset start/middle/end; no fitted alignment',
                    segments=[dict(start_frame=i, end_frame=min(len(r), i+rate),
                        unequal_samples=int(np.count_nonzero(r[i:i+rate] != c[i:i+rate])))
                        for i in (0, len(r)//2, max(0, len(r)-rate))]))
    return report

def compare_events(reference, candidate):
    def read(path):
        def reject(value):
            raise ValueError('Invalid trace number: '+value)
        events = [json.loads(line, parse_constant=reject)
                  for line in pathlib.Path(path).read_text().splitlines() if line.strip()]
        previous = None
        for e in events:
            for key in ('tick', 'ordinal', 'chip', 'register', 'value'):
                if type(e.get(key)) is not int or e[key] < 0:
                    raise ValueError('Invalid event field '+key)
            if e['chip'] > 1 or e['register'] > 13 or e['value'] > 255:
                raise ValueError('Invalid register event range')
            order = (e['tick'], e['ordinal'])
            if previous is not None and order <= previous:
                raise ValueError('Unordered/duplicate event')
            previous = order
        return events
    r, c = read(reference), read(candidate)
    first = next((i for i, (a, b) in enumerate(zip(r, c)) if a != b), None)
    if first is None and len(r) != len(c):
        first = min(len(r), len(c))
    return dict(status='EXACT_ORDERED_EVENTS' if first is None else 'FAIL_EVENTS',
                reference_events=len(r), candidate_events=len(c), first_divergence=first,
                reference_sha256=hashlib.sha256(pathlib.Path(reference).read_bytes()).hexdigest(),
                candidate_sha256=hashlib.sha256(pathlib.Path(candidate).read_bytes()).hexdigest())

def write_difference(path, r, c, meta):
    """Unscaled signed residual in normalized amplitude, IEEE float32 WAV.
    16-bit differences are exactly representable, including values beyond unity.
    No integer clipping; this derivative is never an acceptance input.
    """
    if r.shape != c.shape or not len(r) or meta['bits'] > 24:
        raise ValueError('residual requires equal nonempty <=24-bit PCM')
    values = ((c.astype(np.int64)-r.astype(np.int64)) / (1 << (meta['bits']-1))).astype('<f4')
    data = values.tobytes()
    ch, rate = meta['channels'], meta['rate_hz']
    fmt = struct.pack('<HHIIHH', 3, ch, rate, rate*ch*4, ch*4, 32)
    body = b'WAVEfmt '+struct.pack('<I', 16)+fmt+b'fact'+struct.pack('<II', 4, len(r))+b'data'+struct.pack('<I', len(data))+data
    pathlib.Path(path).write_bytes(b'RIFF'+struct.pack('<I', len(body))+body)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('reference'); p.add_argument('candidate')
    p.add_argument('--report', required=True); p.add_argument('--difference')
    p.add_argument('--reference-events'); p.add_argument('--candidate-events')
    a = p.parse_args()
    try:
        rm, r = read_wav(a.reference); cm, c = read_wav(a.candidate)
        report = compare(r, c, rm, cm)
        if bool(a.reference_events) != bool(a.candidate_events):
            raise ValueError('Both event traces are required')
        if a.reference_events:
            report['events'] = compare_events(a.reference_events, a.candidate_events)
            report['event_trace_status'] = report['events']['status']
            if report['event_trace_status'] != 'EXACT_ORDERED_EVENTS':
                report.update(pcm_status=report['status'], status='MISMATCH', reason='CONFLICTING_EVENTS')
        if a.difference and report.get('coverage_frames', 0):
            write_difference(a.difference, r, c, rm)
            report['residual'] = dict(encoding='IEEE_float32', gain=1, units='amplitude / reference full scale')
    except (ValueError, OSError) as e:
        report = dict(schema_version=SCHEMA_VERSION, status='MISMATCH', reason='INVALID_OR_MISSING_INPUT', error=str(e))
    report['command'] = sys.argv
    pathlib.Path(a.report).write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print(report['status'])
    return 0 if report['status'] in ACCEPTED else 1

if __name__ == '__main__':
    sys.exit(main())
