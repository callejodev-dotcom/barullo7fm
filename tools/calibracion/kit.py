#!/usr/bin/env python3
# Kit de calibración: un banco de 32 sons de proba (.syx) e un ficheiro MIDI que os toca.
# Grávase o DX7 real tocando o MIDI e compárase co que fai Barullo 7FM co mesmo MIDI.
import os, struct
here = os.path.dirname(os.path.abspath(__file__))

def op(ol=0, ratio=1, fine=0, det=7, rates=(99,99,99,99), levels=(99,99,99,0), kvs=0, rs=0, ams=0,
       bp=39, ld=0, rd=0, lc=0, rc=0, fixed=0):
    r = list(rates) + list(levels) + [bp, ld, rd, lc, rc, rs, ams, kvs, ol, fixed, ratio, fine, det]
    return r
def voice(name, alg, ops, fb=0, oks=1, lfo=(35,0,0,0,1,0), pms=3, peg=((99,99,99,99),(50,50,50,50)), transp=24):
    # ops: lista OP1..OP6 → nos datos van OP6 primeiro
    d = []
    for o in reversed(ops): d += o
    d += list(peg[0]) + list(peg[1]) + [alg - 1, fb, oks] + list(lfo[:4]) + [lfo[4], lfo[5], pms, transp]
    n = (name + ' ' * 10)[:10]
    d += [ord(c) for c in n]
    assert len(d) == 155, len(d)
    return d

def pack(v):
    out = []
    for o in range(6):
        s = v[o*21:(o+1)*21]
        out += s[0:11]
        out += [(s[12] << 2) | s[11], (s[20] << 3) | s[13], (s[15] << 2) | s[14], s[16], (s[18] << 1) | s[17], s[19]]
    g = v[126:]
    out += g[0:8] + [g[8], (g[10] << 3) | g[9]] + g[11:15] + [(g[17] << 4) | (g[16] << 1) | g[15], g[18]] + g[19:29]
    assert len(out) == 128
    return out

OFF = op(ol=0)
def solo(ol=99, **k): return op(ol=ol, **k)
V = []
# 1–5: nivel de saída dun operador só (seno), algoritmo 32 cun só operador soando
for i, ol in enumerate((99, 90, 70, 50, 30)):
    V.append(voice('T%02d OL %d' % (i + 1, ol), 32, [solo(ol)] + [OFF] * 5))
# 6–10: FM de dous operadores (OP2 → OP1, 1:1), algoritmo 1 → índice de modulación
for i, ol in enumerate((99, 90, 80, 70, 60)):
    V.append(voice('T%02d FM %d' % (i + 6, ol), 1, [solo(99), op(ol=ol)] + [OFF] * 4))
# 11–18: realimentación 0..7 (algoritmo 32, só o OP6)
for fb in range(8):
    V.append(voice('T%02d FB %d' % (fb + 11, fb), 32, [OFF] * 5 + [solo(99)], fb=fb))
# 19: seis portadores (algoritmo 32), razóns 1..6 → atenuación por número de portadores
V.append(voice('T19 COM 6', 32, [op(ol=99, ratio=r) for r in range(1, 7)]))
# 20: un portador no algoritmo 16 (sen moduladores soando)
V.append(voice('T20 ALG16', 16, [solo(99)] + [OFF] * 5))
# 21: envolvente: taxas 50, niveis 99 50 80 0
V.append(voice('T21 EG 50', 32, [op(ol=99, rates=(50,50,50,50), levels=(99,50,80,0))] + [OFF] * 5))
# 22: caída lenta e tramo plano (L1 = L2)
V.append(voice('T22 EG LEN', 32, [op(ol=99, rates=(80,30,20,40), levels=(99,99,60,0))] + [OFF] * 5))
# 23: velocidade (KVS 7)
V.append(voice('T23 VEL 7', 32, [op(ol=99, kvs=7)] + [OFF] * 5))
# 24: escala de nivel de teclado (BP Do3, ambas fondas, curvas −LIN / −EXP)
V.append(voice('T24 ESCALA', 32, [op(ol=99, ld=99, rd=99, lc=0, rc=1)] + [OFF] * 5))
# 25: escala de taxa (RS 7) cunha caída media
V.append(voice('T25 RATE S', 32, [op(ol=99, rs=7, rates=(99,40,99,40), levels=(99,0,0,0))] + [OFF] * 5))
# 26: envolvente de ton
V.append(voice('T26 PITCHE', 32, [solo(99)] + [OFF] * 5, peg=((50,40,60,50),(99,20,60,50))))
# 27: vibrato (seno, velocidade 35, PMD 99, PMS 3)
V.append(voice('T27 VIBRAT', 32, [solo(99)] + [OFF] * 5, lfo=(35,0,99,0,0,4), pms=3))
# 28: trémolo (triángulo, AMD 99, AMS 3)
V.append(voice('T28 TREMOL', 32, [op(ol=99, ams=3)] + [OFF] * 5, lfo=(35,0,0,99,0,0)))
# 29: mostraxe e retención no ton (velocidade 50, PMD 99, PMS 4)
V.append(voice('T29 S/H', 32, [solo(99)] + [OFF] * 5, lfo=(50,0,99,0,0,5), pms=4))
# 30: desafinación: OP1 det 0 (−7) e OP2 det 14 (+7), dous portadores → batemento
V.append(voice('T30 DETUNE', 32, [op(ol=99, det=0), op(ol=99, det=14)] + [OFF] * 4))
# 31: frecuencia fixa 1000 Hz
V.append(voice('T31 FIXA1K', 32, [op(ol=99, fixed=1, ratio=3, fine=0)] + [OFF] * 5))
# 32: sinxelo pero con moita modulación: OP2 → OP1 con realimentación en OP2 (algoritmo 2)
V.append(voice('T32 FM FB', 2, [solo(99), op(ol=85)] + [OFF] * 4, fb=7))
assert len(V) == 32

body = []
for v in V: body += pack(v)
syx = bytes([0xF0, 0x43, 0x00, 0x09, 0x20, 0x00] + body + [(128 - (sum(body) & 127)) & 127, 0xF7])
open(os.path.join(here, 'Barullo7FM-calibracion.syx'), 'wb').write(syx)

# ---- MIDI: formato 0, 480 por negra, 120 bpm → 1 negra = 0,5 s
PPQ = 480
ev = []   # (tick, bytes)
def at(sec): return int(round(sec * 2 * PPQ))
t = 3.0   # 3 s de silencio ao comezo (ruído de fondo)
def note(n, vel, hold, gap):
    global t
    ev.append((at(t), [0x90, n, vel])); ev.append((at(t + hold), [0x80, n, 64])); t += hold + gap
ev.append((0, [0xB0, 121, 0])); ev.append((0, [0xB0, 1, 0])); ev.append((0, [0xE0, 0, 64]))
plan = []
for p in range(32):
    ev.append((at(t), [0xC0, p])); t += 0.5
    name = V[p][145:155]
    if p == 22:      # velocidades
        for vel in (1, 16, 32, 48, 64, 80, 96, 112, 127): note(60, vel, 1.0, 0.5)
    elif p in (23, 24):   # teclado enteiro
        for n in (24, 36, 48, 60, 72, 84, 96): note(n, 100, 1.5, 0.8)
    elif p in (26, 27, 28):   # moduladores: notas longas
        note(60, 100, 5.0, 1.0)
    elif p in (20, 21, 25):   # envolventes
        note(60, 100, 3.0, 3.0)
    else:
        for n in (36, 60, 84): note(n, 100, 1.5, 1.0)
    t += 0.5
end = at(t + 2)
ev.sort(key=lambda e: e[0])
trk = b''
last = 0
def vlq(x):
    b = [x & 0x7F]; x >>= 7
    while x: b.insert(0, (x & 0x7F) | 0x80); x >>= 7
    return bytes(b)
trk += vlq(0) + bytes([0xFF, 0x51, 0x03]) + (500000).to_bytes(3, 'big')
for tk, b in ev:
    trk += vlq(tk - last) + bytes(b); last = tk
trk += vlq(end - last) + bytes([0xFF, 0x2F, 0x00])
mid = b'MThd' + struct.pack('>IHHH', 6, 0, 1, PPQ) + b'MTrk' + struct.pack('>I', len(trk)) + trk
open(os.path.join(here, 'Barullo7FM-calibracion.mid'), 'wb').write(mid)
print('sons: 32, duración do MIDI: %.0f s (%.1f min)' % (t + 2, (t + 2) / 60))

# ---- segundo MIDI: os 32 sons do banco que haxa na memoria interna (pensado para ROM1A),
# unha nota e un acorde cada un, coma a demo de Barullo 7FM
ev = []; t = 2.0
ev.append((0, [0xB0, 121, 0])); ev.append((0, [0xB0, 1, 0])); ev.append((0, [0xE0, 0, 64]))
for p in range(32):
    ev.append((at(t), [0xC0, p])); t += 0.3
    note(60, 100, 1.2, 0.4)
    for n in (55, 64, 71): ev.append((at(t), [0x90, n, 80]))
    for n in (55, 64, 71): ev.append((at(t + 1.6), [0x80, n, 64]))
    t += 1.6 + 1.2
end = at(t + 2)
ev.sort(key=lambda e: e[0])
trk = vlq(0) + bytes([0xFF, 0x51, 0x03]) + (500000).to_bytes(3, 'big'); last = 0
for tk, b in ev:
    trk += vlq(tk - last) + bytes(b); last = tk
trk += vlq(end - last) + bytes([0xFF, 0x2F, 0x00])
mid = b'MThd' + struct.pack('>IHHH', 6, 0, 1, PPQ) + b'MTrk' + struct.pack('>I', len(trk)) + trk
open(os.path.join(here, 'Barullo7FM-32sons.mid'), 'wb').write(mid)
print('32 sons: %.0f s' % (t + 2))
