"""Phổ biến thiên CSI wave vs no_wave (4.1) → docs/features_4-1/10_pho_vay_vs_yen.png + pho_theo_dai.csv.
    .venv/bin/python sensing/analyze_spectrum_41.py   (danh sách lượt ở biến runs)"""
import sys, warnings; warnings.filterwarnings("ignore"); sys.path.insert(0,'sensing')
import numpy as np, pandas as pd, features as F, json
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
runs=sys.argv[1:] or ["20261020-152209_4-1_no_wave_S1_P1","20261020-152701_4-1_no_wave_S1_P1","20261020-152046_4-1_wave_S1_P1","20261020-152845_4-1_wave_S1_P1","20261020-164644_4-1_wave_S1_P1"]
WIN=6.4; FS=20; N=int(WIN*FS); f=np.fft.rfftfreq(N,1/FS); hann=np.hanning(N)
bands={"thở <0.7 Hz":(0.1,0.7),"0.7–1 Hz":(0.7,1.0),"vẫy 1–4 Hz":(1.0,4.0),"nhanh 4–10 Hz":(4.0,10.01)}
res={}; rows=[]
for r in runs:
    rec,h=F.load_recording('data/raw/'+r,iq=True); h=h[:,:3]
    tsf=rec.chip_tsf_us.to_numpy(np.int64); amp=np.abs(h)
    P_amp=[];P_rat=[]
    for piece in F.segments(rec,F.qc_keep(rec),F.TIME_MAX_GAP_US):
        t=(tsf[piece]-tsf[piece[0]])/1e6; s=0.0
        while s+WIN<=t[-1]:
            sel=(t>=s)&(t<s+WIN); idx=piece[sel]
            if sel.sum()>=0.5*WIN/0.0512:
                g=s+np.arange(N)/FS
                ps=[]
                for c in range(3):
                    a=amp[idx,c]/amp[idx,c].mean(1,keepdims=True)
                    x=F._interp_cols(g,t[sel],a); x=x-x.mean(0); tc=np.arange(N)-(N-1)/2; x=x-np.outer(tc,tc@x/np.sum(tc*tc))
                    ps.append((np.abs(np.fft.rfft(x*hann[:,None],axis=0))**2).mean(1))
                P_amp.append(np.mean(ps,0))
                q=h[idx,0]*np.conj(h[idx,1]); u=q/(np.abs(q)+1e-9)
                ug=F._interp_cols(g,t[sel],u.real)+1j*F._interp_cols(g,t[sel],u.imag)
                ph=np.unwrap(np.angle(ug*np.conj(ug.mean(0))),axis=0); ph=ph-ph.mean(0); ph=ph-np.outer(tc,tc@ph/np.sum(tc*tc))
                P_rat.append((np.abs(np.fft.rfft(ph*hann[:,None],axis=0))**2).mean(1))
            s+=1.6
    A=np.array(P_amp); R=np.array(P_rat)
    res[r]=(A.mean(0),R.mean(0))
    lab='wave' if '_wave_' in r and 'no_wave' not in r else 'no_wave'
    for name,X in (("biên độ",A),("CSI-ratio pha",R)):
        tot=X[:,(f>=0.1)].sum(1)
        row=dict(run=r[9:15]+lab[0].upper(),tín_hiệu=name,cửa_sổ=len(X),năng_lượng_tb=f"{np.median(tot):.3g}")
        for b,(lo,hi) in bands.items(): row[b]=f"{np.median(X[:,(f>=lo)&(f<hi)].sum(1)/tot):.0%}"
        rows.append(row)
T=pd.DataFrame(rows); print(T.to_string(index=False))
fig,ax=plt.subplots(1,2,figsize=(13,4.5))
for r,(a,ra) in res.items():
    w='no_wave' not in r; st=dict(color='#d1495b' if w else ('#6a4c93' if r.startswith('20261020-171141') else '#00798c'),ls='-' if w else '--')
    ax[0].semilogy(f[1:],a[1:],label=r[9:15]+('W' if w else 'N'),**st); ax[1].semilogy(f[1:],ra[1:],**st)
for a_,tt in zip(ax,["Biên độ chuẩn hoá (TB 3 chain, 208 tone)","Pha CSI-ratio H0·conj(H1)"]):
    a_.axvspan(0.1,0.7,color='grey',alpha=0.15); a_.text(0.15,a_.get_ylim()[1]*0.5,'thở',fontsize=9)
    a_.axvspan(1,4,color='orange',alpha=0.1); a_.text(1.5,a_.get_ylim()[1]*0.5,'vẫy 1–4 Hz',fontsize=9)
    a_.set_xlim(0,10); a_.set_xlabel('Hz'); a_.set_title(tt)
ax[0].set_ylabel('công suất (TB cửa sổ 6.4 s)'); ax[0].legend(fontsize=8)
fig.suptitle('Phổ biến thiên CSI theo lượt — đỏ liền: wave, xanh đứt: no_wave'); fig.tight_layout()
fig.savefig('docs/features_4-1/10_pho_vay_vs_yen.png',dpi=110)
T.to_csv('docs/features_4-1/pho_theo_dai.csv',index=False)
