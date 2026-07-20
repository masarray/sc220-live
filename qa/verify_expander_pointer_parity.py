import math
import random

random.seed(61850)
sr = 48000.0
n = 4096
left = [random.uniform(-0.04, 0.04) for _ in range(n)]
right = [random.uniform(-0.04, 0.04) for _ in range(n)]
amount = 0.55
floor_db = -72.0
offset_db = 6.0

def smooth_step(x):
    x=max(0.0,min(1.0,x)); return x*x*(3.0-2.0*x)
def gain_to_db(x): return 20.0*math.log10(max(x,1e-12))
def db_to_gain(x): return 10.0**(x/20.0)

def render(pointer_style):
    out_l=left.copy(); out_r=right.copy()
    env=0.0; gain_state=0.0; hold=0; opened=True
    ratio=1.0+(1.75-1.0)*amount
    range_db=30.0*amount
    threshold=max(-86.0,min(-36.0,floor_db+offset_db))
    open_threshold=threshold+3.0
    hold_len=int(sr*(0.145+(0.085-0.145)*amount))
    attack=1.0-math.exp(-1.0/(sr*0.008))
    release=1.0-math.exp(-1.0/(sr*0.085))
    gain_open=1.0-math.exp(-1.0/(sr*0.004))
    gain_close=1.0-math.exp(-1.0/(sr*0.45))
    for i in range(n):
        power=left[i]*left[i]+right[i]*right[i]
        rms=math.sqrt(power/2.0+1e-14)
        alpha=attack if rms>env else release
        env += alpha*(rms-env)
        level=gain_to_db(env)
        if opened:
            if level<threshold:
                if hold>0: hold-=1
                else: opened=False
            else: hold=hold_len
        elif level>open_threshold:
            opened=True; hold=hold_len
        desired=0.0
        if not opened:
            hard=max(-range_db,(ratio-1.0)*(level-threshold))
            ks=threshold-4.0; ke=threshold+4.0
            if level<=ks: desired=hard
            elif level<ke: desired=hard*(1.0-smooth_step((level-ks)/8.0))
        ga=gain_open if desired>gain_state else gain_close
        gain_state += ga*(desired-gain_state)
        gain=db_to_gain(gain_state)
        if pointer_style:
            out_l[i] *= gain; out_r[i] *= gain
        else:
            out_l[i] = out_l[i]*gain; out_r[i] = out_r[i]*gain
    return out_l,out_r

a=render(False); b=render(True)
err=max(max(abs(x-y) for x,y in zip(a[0],b[0])), max(abs(x-y) for x,y in zip(a[1],b[1])))
print(f'Expander pointer-path max abs difference: {err:.3e}')
print('PASS' if err == 0.0 else 'FAIL')
raise SystemExit(0 if err == 0.0 else 1)
