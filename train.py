import pretty_midi as pm, os, json, bisect, collections, math
ROOT='/tmp/data/POP909-Dataset/POP909'
PC={'C':0,'C#':1,'Db':1,'D':2,'D#':3,'Eb':3,'E':4,'F':5,'F#':6,'Gb':6,'G':7,'G#':8,'Ab':8,'A':9,'A#':10,'Bb':10,'B':11,'Cb':11}
MAJ=[0,2,4,5,7,9,11]; MIN=[0,2,3,5,7,8,10]
def metric(b):          # b = beat position in bar (0..4)
    f=b%1.0
    if abs(b)<1e-3 or abs(b-2)<1e-3: return 0   # bar start / half bar
    if f<1e-3: return 1                            # other beats
    if abs(f-0.5)<1e-3: return 2                   # 8ths
    return 3                                        # 16ths and other
trans={m:collections.Counter() for m in ('maj','min')}   # (deg, mcNext, step)
first={m:collections.Counter() for m in ('maj','min')}
end={m:collections.Counter() for m in ('maj','min')}
strong={m:collections.Counter() for m in ('maj','min')}
rhythms=collections.Counter(); durs=collections.Counter()
chrom=collections.Counter(); total=0; songs=0; notes_used=0
for sid in sorted(os.listdir(ROOT)):
    d=os.path.join(ROOT,sid)
    if not os.path.isdir(d): continue
    try:
        m=pm.PrettyMIDI(os.path.join(d,sid+'.mid'))
        mel=[i for i in m.instruments if i.name.upper()=='MELODY'][0]
        beats=[];down=[]
        for line in open(os.path.join(d,'beat_midi.txt')):
            t,a,b=map(float,line.split()); beats.append(t); down.append(b)
        keys=[]
        for line in open(os.path.join(d,'key_audio.txt')):
            s,e,k=line.split(); keys.append((float(s),float(e),k))
    except Exception as ex:
        continue
    # beat index -> bar position using downbeat flags
    di=[i for i,x in enumerate(down) if x==1.0]
    if not di: continue
    def beatpos(t):
        i=bisect.bisect_right(beats,t)-1
        if i<0 or i+1>=len(beats): return None
        frac=(t-beats[i])/(beats[i+1]-beats[i])
        q=round((i+frac)*4)/4       # quantise to 16ths
        return q
    def keyat(t):
        for s,e,k in keys:
            if s-1<=t<=e+1: return k
        return keys[0][2] if keys else None
    ns=sorted(mel.notes,key=lambda n:n.start)
    seq=[]
    for n in ns:
        b=beatpos(n.start); be=beatpos(n.end)
        k=keyat(n.start)
        if b is None or be is None or k is None or ':' not in k: continue
        r,mo=k.split(':'); root=PC.get(r)
        if root is None: continue
        mode='min' if mo.startswith('min') else 'maj'
        sc=MIN if mode=='min' else MAJ
        rel=(n.pitch-root)%12
        if rel not in sc: chrom[mode]+=1; total+=1; seq.append(None); continue
        total+=1
        deg=sc.index(rel); octv=(n.pitch-root)//12
        bar=(b-di[0])%4 if True else 0
        seq.append(dict(step=octv*7+deg,deg=deg,mode=mode,b=b,bar=(b-di[0])%4,len=max(0.25,be-b),bari=int((b-di[0])//4)))
    songs+=1
    # phrases split on rests >= 1 beat or chromatic notes
    phr=[];cur=[]
    for i,x in enumerate(seq):
        if x is None:
            if len(cur)>2: phr.append(cur)
            cur=[];continue
        if cur and x['b']-(cur[-1]['b']+cur[-1]['len'])>=1.0:
            if len(cur)>2: phr.append(cur)
            cur=[]
        cur.append(x)
    if len(cur)>2: phr.append(cur)
    for p in phr:
        mo=p[0]['mode']
        first[mo][p[0]['deg']]+=1; end[mo][p[-1]['deg']]+=1
        for a,b in zip(p,p[1:]):
            s=b['step']-a['step']
            if abs(s)>7: continue
            trans[mo][(a['deg'],metric(b['bar']),s)]+=1; notes_used+=1
        for x in p:
            if metric(x['bar'])==0: strong[mo][x['deg']]+=1
    # rhythms: per bar 16th onset masks
    bars=collections.defaultdict(int)
    for x in seq:
        if x is None: continue
        pos=int(round(x['bar']*4))%16
        bars[x['bari']]|=1<<pos
    for v in bars.values():
        if bin(v).count('1')>=2: rhythms[v]+=1
print('songs',songs,'transitions',notes_used,'chromatic share maj %.3f'%(chrom['maj']/max(1,total)))
out={}
for mo in ('maj','min'):
    t={}
    for (d,mc,s),c in trans[mo].items(): t.setdefault(f'{d}_{mc}',{})[s]=c
    out[mo]=dict(trans=t,first=dict(first[mo]),end=dict(end[mo]),strong=dict(strong[mo]))
out['rhythms']=rhythms.most_common(64)
json.dump(out,open('/home/claude/train/model.json','w'))
for mo in ('maj','min'):
    tot=sum(end[mo].values()); print(mo,'end',{k:round(v/tot,2) for k,v in sorted(end[mo].items())})
    tot=sum(strong[mo].values()); print(mo,'strong',{k:round(v/tot,2) for k,v in sorted(strong[mo].items())})
    allst=collections.Counter()
    for (d,mc,s),c in trans[mo].items(): allst[s]+=c
    tot=sum(allst.values()); print(mo,'steps',{k:round(v/tot,3) for k,v in sorted(allst.items())})
print('top rhythms',[(format(m,'016b')[::-1],c) for m,c in rhythms.most_common(8)])
