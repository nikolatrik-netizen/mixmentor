"""Checks Mix Mentor's listening engine against reference measurements.

Each test makes an audio file where the right answer is known (or measured with a reference
tool: pyloudnorm for ITU BS.1770 loudness, numpy/scipy for everything else), runs the plugin's
own analysis code on it (the `ah` harness), and compares.
"""
import json, subprocess, sys, numpy as np, soundfile as sf, pyloudnorm as pyln
from scipy import signal

HARNESS = '/tmp/ah/b/ah_artefacts/Release/ah'
rng = np.random.default_rng(1)
results = []

def run(x, sr, seconds=4.0):
    if x.ndim == 1: x = np.stack([x, x], 1)
    sf.write('/tmp/ah/t.wav', x.astype(np.float32), sr, subtype='FLOAT')
    out = subprocess.run([HARNESS, '/tmp/ah/t.wav', str(seconds)], capture_output=True, text=True).stdout
    return json.loads(out)

def check(name, measured, expected, tol, unit='dB'):
    ok = abs(measured - expected) <= tol
    results.append(ok)
    print(f"{'PASS' if ok else 'FAIL'}  {name}: measured {measured:.2f}{unit}, expected {expected:.2f}{unit} (+-{tol})")

def pink(n, sr):
    w = rng.standard_normal(n)
    f = np.fft.rfftfreq(n, 1/sr); spec = np.fft.rfft(w); spec[1:] /= np.sqrt(f[1:]); spec[0] = 0
    x = np.fft.irfft(spec, n); return x / np.max(np.abs(x)) * 0.5

def peaking(f0, gain_db, q, sr):   # RBJ peaking EQ (same as the EQ a user would use)
    A = 10**(gain_db/40); w0 = 2*np.pi*f0/sr; al = np.sin(w0)/(2*q)
    b = [1+al*A, -2*np.cos(w0), 1-al*A]; a = [1+al/A, -2*np.cos(w0), 1-al/A]
    return np.array(b)/a[0], np.array(a)/a[0]

def eq_avg_db(f0, g, q, sr, lo, hi):
    """Average gain (dB) of an EQ bell across a zone, on a log-frequency grid (what a perfect analyser would see)."""
    b, a = peaking(f0, g, q, sr); f = np.geomspace(lo, hi, 200)
    _, h = signal.freqz(b, a, worN=f, fs=sr); return np.mean(20*np.log10(np.abs(h)))

for sr in (44100, 48000, 96000):
    print(f'\n=== sample rate {sr} ===')
    n = int(sr * 6)

    # 1. Loudness (pyloudnorm = independent BS.1770 implementation)
    x = pink(n, sr) * 0.6
    st = np.stack([x, np.roll(x, 37)], 1)
    ref = pyln.Meter(sr).integrated_loudness(st[:int(sr*4.4)])
    check('loudness, pink noise (vs pyloudnorm)', run(st, sr)['lufs'], ref, 0.5, ' LUFS')

    t = np.arange(n)/sr
    music = 0.3*np.sin(2*np.pi*110*t)*(1+0.5*np.sin(2*np.pi*2*t)) + 0.1*pink(n, sr) + 0.05*np.sin(2*np.pi*2500*t)
    ref = pyln.Meter(sr).integrated_loudness(np.stack([music, music], 1)[:int(sr*4.4)])
    check('loudness, tonal + noise (vs pyloudnorm)', run(music, sr)['lufs'], ref, 0.5, ' LUFS')

    # 2. True peak: a sine at fs/4 sampled 45 degrees off its crest has sample peaks at -3 dB but a 0 dB true peak
    tp = np.sin(2*np.pi*(sr/4)*t + np.pi/4) * 0.9
    check('true peak (inter-sample, sample peak is -3.9 dB)', run(tp, sr)['tp'], 20*np.log10(0.9), 0.5)

    # 3. Stereo correlation
    a = pink(n, sr); bnoise = pink(n, sr)
    l, r = a, 0.6*a + 0.8*bnoise
    expected = np.sum(l*r)/np.sqrt(np.sum(l*l)*np.sum(r*r))
    check('correlation, partly correlated stereo', run(np.stack([l, r], 1), sr)['corr'], expected, 0.05, '')
    sub_l = 0.4*np.sin(2*np.pi*55*t); sub_r = 0.4*np.sin(2*np.pi*55*t + 2.2)
    check('low-end correlation, sub 126 deg out of phase', run(np.stack([sub_l + 0.05*a, sub_r + 0.05*a], 1), sr)['lowcorr'], np.cos(2.2), 0.08, '')
    check('correlation, fully out of phase', run(np.stack([a, -a], 1), sr)['corr'], -1.0, 0.02, '')

    # 4. Wide stereo doesn't cancel in the spectrum (old version summed to mono: L - L = silence)
    res = run(np.stack([a*0.3, -a*0.3], 1), sr)
    res_mono = run(a*0.3, sr)
    check('out-of-phase stereo reads as loud as mono (box zone level)', res['zones']['box']['level'], res_mono['zones']['box']['level'], 0.5)

    # 5. Build-ups: pink noise with a +6 dB bell at 300 Hz (Q 1.4) -> the boxiness zone (200-400 Hz) must show it
    base = pink(n, sr)
    flat = run(base, sr)['zones']['box']
    b, aa = peaking(300, 6, 1.4, sr); bumped = run(signal.lfilter(b, aa, base), sr)['zones']['box']
    check('EQ +6 dB at 300 Hz: zone level rise', bumped['iso'] - flat['iso'], eq_avg_db(300, 6, 1.4, sr, 200, 400), 0.6)
    check('EQ +6 dB at 300 Hz: bump vs neighbours is clearly positive', bumped['bump'] - flat['bump'], 3.0, 1.5)

    # 6. EQ cut tracking: a -4 dB cut must read as about -4 dB averaged over the zone
    b, aa = peaking(300, -4, 1.0, sr); cut = run(signal.lfilter(b, aa, base), sr)['zones']['box']
    check('EQ -4 dB cut at 300 Hz is heard (zone change)', cut['iso'] - flat['iso'], eq_avg_db(300, -4, 1.0, sr, 200, 400), 0.6)
    b, aa = peaking(55, -6, 1.5, sr); low = run(signal.lfilter(b, aa, base), sr)['zones']['sub']
    check('EQ -6 dB cut at 55 Hz (deep low end) is heard', low['iso'] - run(base, sr)['zones']['sub']['iso'], eq_avg_db(55, -6, 1.5, sr, 40, 70), 0.8)

    # 7. Same sound twice -> same answer (repeatability)
    r1 = run(base, sr)['zones']['box']['bump']; r2 = run(np.roll(base, sr//3), sr)['zones']['box']['bump']
    check('repeatability (two listens of the same sound)', r1, r2, 0.5)

    # 8. Resonance: a steady 2.4 kHz ring in noise must be found; a melody's harmonics must not
    ring = base + 0.03*np.sin(2*np.pi*2400*t)
    rz = run(ring, sr)['zones']['pres']
    check('steady ring at 2.4 kHz is found', rz['resHz'], 2400, 15, ' Hz')
    notes = [220, 247, 262, 294, 330, 294, 262, 247]
    mel = np.concatenate([signal.sawtooth(2*np.pi*f*np.arange(int(sr*0.5))/sr)*0.3 for f in notes] * 2)[:n]
    mz = run(mel + 0.02*base[:len(mel)], sr)['zones']
    found = [z for z in ('box', 'mid', 'pres') if mz[z]['resHz'] > 0]
    check('melody notes are NOT reported as resonances (zones flagged)', len(found), 0, 0, '')

    # 9. Gating: silence between hits doesn't dilute the analysis
    hits = np.zeros(n); seg = (pink(int(sr*0.25), sr)*np.exp(-np.arange(int(sr*0.25))/sr*12))
    for s in range(0, n - len(seg), sr): hits[s:s+len(seg)] += seg
    full = run(np.tile(seg, n//len(seg)+1)[:n], sr, 1.2)['zones']['mid']['level']
    gated = run(hits, sr, 1.2)['zones']['mid']['level']
    check('gaps between hits are ignored (level vs. no gaps)', gated, full, 3.0)

print(f'\n{sum(results)}/{len(results)} checks passed')
sys.exit(0 if all(results) else 1)
