#!/usr/bin/env python3
# Banco de sons propio de Barullo 7FM (32 sons deseñados de cero, non copiados de ningún banco).
# Xera Source/Banco.h (para o plugin) e Resources/Barullo7FM-Banco.syx (para calquera DX7 ou emulador).
import sys, os

def op(ratio=1.0, ol=0, rates=(99,99,99,99), levels=(99,99,99,0), kvs=0, rs=0, ams=0,
       det=7, fixed=False, bp=39, ld=0, rd=0, lc=0, rc=0):
    """Un operador. ratio: 0.5, 1, 2… con decimais (fine). fixed=True: ratio en Hz."""
    if fixed:
        # frecuencia fixa: 10^(coarse&3) * (1 + fine/100)  (aprox.)
        import math
        e = int(math.floor(math.log10(ratio))); e = max(0, min(3, e))
        fine = int(round((ratio / 10**e - 1) * 100)); fine = max(0, min(99, fine))
        m, fc, ff = 1, e, fine
    else:
        if ratio < 1:
            fc, ff = 0, int(round((ratio / 0.5 - 1) * 100))
        else:
            fc = int(ratio); ff = int(round((ratio / fc - 1) * 100))
        m = 0
        ff = max(0, min(99, ff))
    return dict(r=list(rates), l=list(levels), bp=bp, ld=ld, rd=rd, lc=lc, rc=rc, rs=rs, ams=ams, kvs=kvs,
                ol=ol, m=m, fc=fc, ff=ff, det=det)

def patch(name, alg, ops, fb=0, oks=1, lfo=(35, 0, 0, 0, 0, 0), pms=3, transp=24,
          pr=(99, 99, 99, 99), pl=(50, 50, 50, 50)):
    """alg 1..32 como no panel; ops = [OP1, OP2, …, OP6]; lfo = (veloc, retardo, PMD, AMD, sync, onda)."""
    assert len(ops) == 6 and len(name) <= 10, name
    b = []
    for o in reversed(ops):           # nos datos vai primeiro o OP6
        b += o['r'] + o['l'] + [o['bp'], o['ld'], o['rd'], o['lc'], o['rc'], o['rs'], o['ams'], o['kvs'],
                                o['ol'], o['m'], o['fc'], o['ff'], o['det']]
    b += list(pr) + list(pl) + [alg - 1, fb, oks] + [lfo[0], lfo[1], lfo[2], lfo[3], lfo[4], lfo[5], pms, transp]
    b += [ord(c) for c in name.ljust(10)]
    assert len(b) == 155
    return b

OFF = op(ol=0)
P = []   # (nome, bytes, descrición en galego)

def add(name, desc, *a, **k):
    P.append((name, patch(name, *a, **k), desc))

# ---------------------------------------------------------------- teclas
ep_car = dict(rates=(96, 25, 25, 67), levels=(99, 75, 0, 0), kvs=2, rs=3)
add("PIANO ELEC", "Piano eléctrico de lingüetas: corpo suave e un «tin» metálico que depende da forza.", 5, [
    op(1, 99, **ep_car),
    op(1, 70, (95, 50, 35, 78), (99, 60, 0, 0), kvs=6, rs=2),
    op(1, 92, (95, 29, 20, 50), (99, 95, 0, 0), kvs=2, rs=3, det=9),
    op(14, 52, (98, 62, 28, 78), (99, 0, 0, 0), kvs=7, rs=3),
    op(1, 80, (95, 30, 20, 50), (99, 90, 0, 0), kvs=1, rs=3, det=5),
    op(1, 52, (95, 40, 30, 70), (99, 55, 0, 0), kvs=5),
], fb=4)
add("PIANO CRIS", "Piano eléctrico máis brillante e de cristal, para baladas.", 5, [
    op(1, 99, (96, 22, 20, 62), (99, 80, 0, 0), kvs=2, rs=3),
    op(1, 78, (95, 45, 30, 75), (99, 55, 0, 0), kvs=6, rs=2),
    op(3, 80, (97, 35, 25, 60), (99, 70, 0, 0), kvs=3, rs=3),
    op(14, 66, (99, 70, 30, 78), (99, 0, 0, 0), kvs=7, rs=4),
    op(1, 85, (96, 26, 20, 55), (99, 85, 0, 0), kvs=2, rs=3, det=10),
    op(5, 50, (98, 55, 30, 75), (99, 40, 0, 0), kvs=6),
], fb=2)
add("CLAVINET", "Clavinet funky: ataque seco e nasal, morre rápido.", 5, [
    op(1, 99, (99, 45, 30, 75), (99, 70, 0, 0), kvs=3, rs=3),
    op(3, 82, (99, 55, 40, 80), (99, 55, 0, 0), kvs=6, rs=3),
    op(1, 90, (99, 48, 30, 75), (99, 65, 0, 0), kvs=3, rs=3, det=8),
    op(1, 84, (99, 60, 40, 80), (99, 50, 0, 0), kvs=6, rs=3),
    op(2, 70, (99, 50, 30, 75), (99, 60, 0, 0), kvs=2, rs=3),
    op(5, 70, (99, 70, 40, 80), (99, 30, 0, 0), kvs=5),
], fb=5)
add("CLAVECIN", "Clavicémbalo: moitos harmónicos, punteado e sen sustain.", 5, [
    op(1, 99, (99, 30, 25, 70), (99, 80, 0, 0), kvs=1, rs=4),
    op(4, 80, (99, 40, 30, 75), (99, 65, 0, 0), kvs=3, rs=4),
    op(2, 85, (99, 33, 25, 70), (99, 75, 0, 0), kvs=1, rs=4, det=9),
    op(5, 84, (99, 45, 30, 75), (99, 60, 0, 0), kvs=3, rs=4),
    op(1, 80, (99, 35, 25, 70), (99, 70, 0, 0), kvs=1, rs=4, det=5),
    op(7, 72, (99, 50, 30, 75), (99, 40, 0, 0), kvs=2),
], fb=6)
org = dict(rates=(99, 80, 99, 85), levels=(99, 99, 99, 0))
add("ORGANO 1", "Órgano de rodas: seis tiradores (16', 8', 4', 2 2/3', 2', 1').", 32, [
    op(0.5, 92, **org), op(1, 95, **org), op(2, 86, **org), op(3, 80, **org), op(4, 78, **org), op(8, 70, **org),
], fb=0, lfo=(36, 0, 6, 0, 0, 4), pms=2)
add("ORGANO PER", "Órgano de jazz con clic de percusión ao premer.", 32, [
    op(0.5, 88, **org), op(1, 95, **org), op(2, 84, **org),
    op(3, 86, (99, 55, 99, 85), (99, 0, 0, 0)), op(4, 72, **org),
    op(10, 70, (99, 85, 99, 90), (99, 0, 0, 0), kvs=2),
])
add("ORGANO IGR", "Órgano de igrexa: entra amodo e cheo, con tubos de 16' a 1'.", 32, [
    op(0.5, 90, (60, 80, 99, 55), (99, 99, 99, 0)), op(1, 96, (62, 80, 99, 55), (99, 99, 99, 0)),
    op(2, 88, (64, 80, 99, 55), (99, 99, 99, 0)), op(4, 82, (66, 80, 99, 55), (99, 99, 99, 0)),
    op(6, 72, (68, 80, 99, 55), (99, 99, 99, 0)), op(8, 74, (70, 80, 99, 55), (99, 99, 99, 0), det=9),
], lfo=(30, 0, 4, 0, 0, 4), pms=1)
add("HARMONIO", "Harmonio de lingüetas, con aire.", 1, [
    op(1, 99, (70, 50, 99, 60), (99, 95, 95, 0)),
    op(1, 76, (70, 50, 99, 60), (99, 90, 90, 0), kvs=2),
    op(1, 90, (68, 50, 99, 60), (99, 95, 95, 0), det=9),
    op(3, 70, (70, 50, 99, 60), (99, 90, 90, 0)),
    OFF, OFF,
], fb=0, lfo=(40, 20, 5, 0, 0, 4), pms=2)
# ---------------------------------------------------------------- metais e vento
add("METAIS 1", "Sección de metais: o brillo abre ao atacar, como un soplo.", 22, [
    op(1, 99, (72, 35, 30, 60), (99, 90, 88, 0), kvs=2),
    op(1, 80, (60, 30, 25, 55), (99, 86, 82, 0), kvs=4),
    op(1, 93, (70, 35, 30, 60), (99, 90, 88, 0), det=9),
    op(1, 93, (70, 35, 30, 60), (99, 90, 88, 0), det=5),
    op(2, 75, (70, 35, 30, 60), (99, 90, 88, 0), det=7),
    op(1, 78, (58, 30, 25, 55), (99, 86, 80, 0), kvs=4),
], fb=6, lfo=(35, 30, 6, 0, 0, 4), pms=3, pr=(84, 95, 95, 60), pl=(45, 50, 50, 50))
add("TROMPETA", "Trompeta: ataque cun pequeno ascenso de ton e vibrato que entra tarde.", 16, [
    op(1, 99, (75, 40, 30, 65), (99, 92, 90, 0), kvs=2),
    op(1, 82, (62, 30, 25, 60), (99, 88, 84, 0), kvs=5),
    op(1, 70, (70, 40, 30, 65), (99, 85, 80, 0)),
    op(3, 60, (70, 40, 30, 65), (99, 80, 75, 0)),
    op(1, 74, (64, 35, 30, 65), (99, 86, 80, 0), kvs=4),
    op(1, 66, (70, 40, 30, 65), (99, 82, 78, 0)),
], fb=6, lfo=(38, 45, 8, 0, 0, 4), pms=3, pr=(90, 80, 99, 70), pl=(44, 50, 50, 50))
add("SAXO", "Saxo tenor con ronquido ao soprar forte.", 7, [
    op(1, 99, (70, 40, 30, 62), (99, 92, 90, 0), kvs=2),
    op(1, 78, (60, 35, 25, 60), (99, 86, 82, 0), kvs=5),
    op(1, 80, (70, 40, 30, 62), (99, 90, 88, 0), det=8),
    op(3, 64, (62, 35, 30, 60), (99, 80, 76, 0), kvs=4),
    op(1, 62, (62, 35, 30, 60), (99, 80, 76, 0), kvs=4),
    op(1, 60, (70, 40, 30, 62), (99, 85, 80, 0)),
], fb=5, lfo=(36, 40, 6, 0, 0, 4), pms=3)
add("FRAUTA", "Frauta: case senoidal, con aire no ataque.", 5, [
    op(1, 99, (60, 40, 99, 60), (99, 95, 95, 0), kvs=1),
    op(1, 48, (55, 40, 99, 60), (99, 80, 75, 0), kvs=4),
    op(2, 40, (70, 40, 99, 60), (99, 90, 85, 0)),
    op(1, 30, (70, 40, 99, 60), (99, 80, 75, 0)),
    op(9.5, 46, (90, 55, 99, 60), (99, 40, 30, 0)),   # sopro: ruído suave
    op(1, 99, (99, 99, 99, 99), (99, 99, 99, 0)),
], fb=7, lfo=(36, 50, 6, 0, 0, 4), pms=3)
add("CLARINETE", "Clarinete: harmónicos impares, son oco.", 1, [
    op(1, 99, (66, 40, 99, 60), (99, 95, 95, 0), kvs=1),
    op(2, 76, (60, 40, 99, 60), (99, 90, 88, 0), kvs=4),
    op(3, 50, (66, 40, 99, 60), (99, 90, 90, 0)),
    op(2, 50, (66, 40, 99, 60), (99, 85, 85, 0)),
    OFF, OFF,
], fb=0, lfo=(36, 50, 5, 0, 0, 4), pms=3)
add("OBOE", "Óboe: nasal e con corpo, formante arredor dos 1,5 kHz.", 1, [
    op(1, 99, (70, 40, 99, 60), (99, 95, 95, 0), kvs=1),
    op(1, 82, (65, 40, 99, 60), (99, 92, 90, 0), kvs=4),
    op(4, 72, (70, 40, 99, 60), (99, 92, 92, 0)),
    op(1, 70, (66, 40, 99, 60), (99, 90, 88, 0), kvs=3),
    OFF, OFF,
], fb=0, lfo=(38, 45, 6, 0, 0, 4), pms=3)
# ---------------------------------------------------------------- cordas
pad_att = dict(rates=(45, 30, 30, 45), levels=(99, 95, 95, 0))
add("CORDAS", "Sección de cordas: ataque lento, varias cordas lixeiramente desafinadas.", 2, [
    op(1, 92, kvs=1, **pad_att), op(1, 72, (40, 30, 30, 45), (99, 92, 92, 0), kvs=3),
    op(1, 92, det=10, **pad_att), op(2, 60, det=4, **pad_att),
    op(1, 60, (40, 30, 30, 45), (99, 90, 90, 0)), op(1, 70, (40, 30, 30, 45), (99, 90, 90, 0)),
], fb=4, lfo=(34, 35, 7, 0, 0, 4), pms=2)
add("PAD QUENTE", "Colchón quente e ancho, tres capas desafinadas.", 5, [
    op(1, 94, (35, 30, 30, 40), (99, 95, 95, 0)), op(1, 66, (30, 25, 30, 40), (99, 90, 85, 0)),
    op(1, 92, (35, 30, 30, 40), (99, 95, 95, 0), det=11), op(2, 58, (30, 25, 30, 40), (99, 90, 85, 0)),
    op(1, 92, (35, 30, 30, 40), (99, 95, 95, 0), det=3), op(1, 60, (30, 25, 30, 40), (99, 88, 85, 0)),
], fb=3, lfo=(30, 20, 5, 0, 0, 0), pms=2)
add("VIOLIN", "Violín solista con vibrato expresivo.", 2, [
    op(1, 99, (62, 40, 30, 55), (99, 95, 93, 0), kvs=2), op(1, 80, (55, 35, 30, 55), (99, 92, 90, 0), kvs=4),
    op(1, 80, (62, 40, 30, 55), (99, 92, 90, 0), det=9), op(3, 66, (60, 40, 30, 55), (99, 88, 86, 0)),
    op(2, 62, (60, 40, 30, 55), (99, 86, 84, 0)), op(1, 68, (60, 40, 30, 55), (99, 86, 84, 0)),
], fb=5, lfo=(40, 45, 10, 0, 0, 4), pms=3)
pluck = dict(kvs=2, rs=3)
add("ARPA", "Arpa: punteo redondo que vai morrendo amodo.", 5, [
    op(1, 99, (99, 30, 22, 45), (99, 75, 0, 0), **pluck), op(1, 66, (99, 45, 30, 50), (99, 50, 0, 0), kvs=5),
    op(1, 90, (99, 32, 22, 45), (99, 70, 0, 0), det=9, **pluck), op(3, 54, (99, 55, 30, 50), (99, 30, 0, 0), kvs=5),
    op(2, 72, (99, 35, 22, 45), (99, 60, 0, 0), **pluck), op(1, 50, (99, 50, 30, 50), (99, 40, 0, 0)),
], fb=2)
add("GUITARRA", "Guitarra de nailon: punteo cálido.", 5, [
    op(1, 99, (99, 35, 25, 55), (99, 75, 0, 0), **pluck), op(1, 74, (99, 50, 30, 60), (99, 55, 0, 0), kvs=5),
    op(2, 86, (99, 40, 25, 55), (99, 65, 0, 0), **pluck), op(3, 64, (99, 60, 35, 60), (99, 35, 0, 0), kvs=5),
    op(1, 82, (99, 38, 25, 55), (99, 70, 0, 0), det=9, **pluck), op(5, 56, (99, 70, 40, 60), (99, 20, 0, 0)),
], fb=3)
add("KOTO", "Corda punteada xaponesa: ataque metálico e afinación dura.", 5, [
    op(1, 99, (99, 40, 28, 55), (99, 70, 0, 0), **pluck), op(3, 76, (99, 55, 35, 60), (99, 45, 0, 0), kvs=5),
    op(1, 86, (99, 42, 28, 55), (99, 65, 0, 0), det=10, **pluck), op(5, 66, (99, 65, 35, 60), (99, 30, 0, 0), kvs=6),
    op(2, 70, (99, 45, 28, 55), (99, 60, 0, 0), **pluck), op(7, 56, (99, 75, 40, 60), (99, 10, 0, 0)),
], fb=5)
# ---------------------------------------------------------------- baixos
add("BAIXO FM", "O baixo FM clásico dos 80: golpe redondo e brillo que se pecha.", 1, [
    op(0.5, 99, (99, 40, 25, 70), (99, 85, 0, 0), kvs=2, rs=2),
    op(0.5, 86, (99, 55, 30, 70), (99, 60, 0, 0), kvs=5, rs=2),
    op(0.5, 80, (99, 45, 25, 70), (99, 75, 0, 0), kvs=2, rs=2, det=9),
    op(1, 72, (99, 60, 30, 70), (99, 50, 0, 0), kvs=5),
    op(1, 66, (99, 65, 30, 70), (99, 30, 0, 0)),
    op(0.5, 60, (99, 70, 30, 70), (99, 40, 0, 0)),
], fb=4, transp=12)
add("BAIXO SLAP", "Baixo slap: golpe brillante e moi percusivo.", 5, [
    op(0.5, 99, (99, 50, 30, 70), (99, 80, 0, 0), kvs=3, rs=2),
    op(0.5, 88, (99, 65, 35, 70), (99, 50, 0, 0), kvs=6),
    op(1, 84, (99, 55, 30, 70), (99, 70, 0, 0), kvs=3, rs=2),
    op(3, 76, (99, 75, 40, 70), (99, 20, 0, 0), kvs=7),
    op(0.5, 70, (99, 50, 30, 70), (99, 70, 0, 0), det=9),
    op(7, 60, (99, 85, 50, 70), (99, 0, 0, 0), kvs=7),
], fb=6, transp=12)
add("BAIXO SINT", "Baixo sintético de dente de serra (realimentación ao máximo).", 1, [
    op(0.5, 99, (99, 50, 40, 80), (99, 88, 80, 0), kvs=1),
    op(0.5, 90, (99, 45, 35, 80), (99, 80, 70, 0), kvs=4),
    op(0.5, 92, (99, 50, 40, 80), (99, 88, 80, 0), det=9),
    op(0.5, 80, (99, 50, 40, 80), (99, 85, 80, 0)),
    op(0.5, 70, (99, 50, 40, 80), (99, 85, 80, 0)),
    op(0.5, 85, (99, 50, 40, 80), (99, 90, 85, 0)),
], fb=7, transp=12)
# ---------------------------------------------------------------- láminas e campás
add("MARIMBA", "Marimba: golpe de madeira e nota curta.", 5, [
    op(1, 99, (99, 50, 35, 60), (99, 0, 0, 0), kvs=2, rs=4),
    op(4, 70, (99, 75, 40, 70), (99, 0, 0, 0), kvs=6, rs=4),
    op(1, 82, (99, 52, 35, 60), (99, 0, 0, 0), kvs=2, rs=4, det=9),
    op(10, 56, (99, 85, 50, 70), (99, 0, 0, 0), kvs=6),
    op(4, 50, (99, 60, 40, 60), (99, 0, 0, 0)),
    OFF,
], fb=0)
add("VIBRAFONO", "Vibráfono co motor acendido: trémolo suave.", 5, [
    op(1, 99, (99, 32, 25, 40), (99, 70, 0, 0), kvs=2, rs=3, ams=2),
    op(4, 62, (99, 50, 30, 50), (99, 20, 0, 0), kvs=6),
    op(1, 86, (99, 34, 25, 40), (99, 65, 0, 0), kvs=2, rs=3, det=9, ams=2),
    op(10, 50, (99, 70, 40, 60), (99, 0, 0, 0), kvs=6),
    op(4, 50, (99, 40, 30, 40), (99, 40, 0, 0), ams=2),
    OFF,
], lfo=(40, 0, 0, 40, 0, 4), pms=0)
add("CAMPAS", "Campás tubulares: harmónicos inarmónicos e unha cola longuísima.", 5, [
    op(1, 99, (99, 20, 18, 30), (99, 70, 0, 0), kvs=2, rs=2),
    op(3.5, 78, (99, 25, 20, 35), (99, 60, 0, 0), kvs=5),
    op(2, 80, (99, 22, 18, 30), (99, 65, 0, 0), kvs=2, rs=2, det=9),
    op(5.19, 70, (99, 30, 20, 35), (99, 50, 0, 0), kvs=5),
    op(1, 70, (99, 24, 18, 30), (99, 60, 0, 0), det=5),
    op(1.41, 64, (99, 30, 20, 35), (99, 50, 0, 0)),
], fb=0)
add("CELESTA", "Celesta: campaíñas finas e doces.", 5, [
    op(2, 99, (99, 35, 25, 45), (99, 60, 0, 0), kvs=2, rs=4),
    op(4, 64, (99, 50, 30, 55), (99, 30, 0, 0), kvs=5),
    op(4, 80, (99, 38, 25, 45), (99, 55, 0, 0), kvs=2, rs=4, det=9),
    op(14, 52, (99, 70, 40, 60), (99, 0, 0, 0), kvs=6),
    op(2, 76, (99, 36, 25, 45), (99, 50, 0, 0), det=5),
    op(7, 50, (99, 60, 40, 55), (99, 10, 0, 0)),
])
add("BIDON", "Tambor de aceiro do Caribe: golpe metálico afinado.", 5, [
    op(1, 99, (99, 38, 30, 50), (99, 60, 0, 0), kvs=3, rs=3),
    op(1.41, 74, (99, 55, 35, 55), (99, 30, 0, 0), kvs=5),
    op(2, 80, (99, 40, 30, 50), (99, 55, 0, 0), kvs=3, rs=3),
    op(2.83, 64, (99, 60, 35, 55), (99, 20, 0, 0), kvs=5),
    op(3, 60, (99, 45, 30, 50), (99, 45, 0, 0)),
    op(4.24, 54, (99, 65, 40, 55), (99, 10, 0, 0)),
], fb=2)
# ---------------------------------------------------------------- sintes e efectos
add("SINT LEAD", "Solista sintético: serra brillante con vibrato na roda.", 1, [
    op(1, 99, (99, 50, 40, 70), (99, 92, 90, 0), kvs=1),
    op(1, 92, (99, 45, 35, 70), (99, 86, 80, 0), kvs=3),
    op(1, 92, (99, 50, 40, 70), (99, 92, 90, 0), det=10),
    op(1, 84, (99, 50, 40, 70), (99, 88, 85, 0)),
    op(1, 76, (99, 50, 40, 70), (99, 88, 85, 0)),
    op(1, 88, (99, 50, 40, 70), (99, 90, 88, 0)),
], fb=7, lfo=(42, 0, 0, 0, 0, 4), pms=3)
add("SINT PAD", "Pad que se abre amodo mentres mantés a tecla.", 1, [
    op(1, 96, (30, 25, 30, 35), (99, 95, 95, 0)),
    op(1, 82, (20, 18, 30, 35), (99, 92, 90, 0)),
    op(1, 92, (30, 25, 30, 35), (99, 95, 95, 0), det=11),
    op(2, 78, (18, 15, 30, 35), (99, 92, 90, 0)),
    op(1, 70, (20, 18, 30, 35), (99, 88, 86, 0)),
    op(1, 76, (25, 20, 30, 35), (99, 90, 88, 0)),
], fb=5, lfo=(25, 0, 0, 18, 0, 0), pms=1)
add("CORO", "Coro de voces «aah», con formantes.", 5, [
    op(1, 99, (55, 35, 30, 50), (99, 95, 93, 0), kvs=1), op(1, 64, (50, 35, 30, 50), (99, 90, 88, 0)),
    op(2, 80, (55, 35, 30, 50), (99, 93, 92, 0), det=9), op(1, 52, (50, 35, 30, 50), (99, 88, 86, 0)),
    op(5, 66, (55, 35, 30, 50), (99, 90, 88, 0), det=5), op(1, 46, (50, 35, 30, 50), (99, 86, 84, 0)),
], fb=0, lfo=(36, 40, 8, 0, 0, 4), pms=3)
add("ESPAZO", "Efecto espacial: ton aleatorio e campás desafinadas.", 19, [
    op(1, 99, (60, 25, 20, 30), (99, 85, 70, 0)),
    op(3.5, 70, (60, 25, 20, 30), (99, 80, 70, 0)),
    op(7.07, 66, (60, 25, 20, 30), (99, 80, 70, 0)),
    op(1, 86, (60, 25, 20, 30), (99, 85, 70, 0), det=12),
    op(2.5, 84, (60, 25, 20, 30), (99, 85, 70, 0), det=2),
    op(1.41, 72, (60, 25, 20, 30), (99, 80, 70, 0)),
], fb=6, lfo=(55, 0, 30, 0, 0, 5), pms=4)

assert len(P) == 32, len(P)

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# --- cabeceira C++
lines = ["// Xerado por tools/banco.py. Non editar á man.", "#pragma once", "#include <cstdint>", "namespace b7 {",
         "struct FactoryVoice { const char* desc; uint8_t data[155]; };", "static const FactoryVoice FACTORY[32] = {"]
def esc(s):
    return '"' + ''.join(c if ord(c) < 128 else ''.join('\\x%02x' % b for b in c.encode()) + '""' for c in s) + '"'
for name, b, desc in P:
    lines.append("  { %s, { %s } }," % (esc(desc), ",".join(str(x) for x in b)))
lines += ["};", "}"]
open(os.path.join(root, "Source", "Banco.h"), "w").write("\n".join(lines) + "\n")
# --- .syx (banco VMEM de 32)
def pack(v):
    o = []
    for k in range(6):
        s = v[k*21:(k+1)*21]
        o += s[0:11] + [(s[12] << 2) | s[11], (s[20] << 3) | s[13], (s[15] << 2) | s[14], s[16], (s[18] << 1) | s[17], s[19]]
    o += v[126:134] + [v[134], (v[136] << 3) | v[135]] + v[137:141] + [(v[143] << 4) | (v[142] << 1) | v[141], v[144]] + v[145:155]
    assert len(o) == 128
    return o
body = sum((pack(b) for _, b, _ in P), [])
cs = (128 - (sum(body) & 127)) & 127
syx = bytes([0xF0, 0x43, 0x00, 0x09, 0x20, 0x00] + body + [cs, 0xF7])
os.makedirs(os.path.join(root, "Resources"), exist_ok=True)
open(os.path.join(root, "Resources", "Barullo7FM-Banco.syx"), "wb").write(syx)
print("32 sons:", ", ".join(n for n, _, _ in P))
