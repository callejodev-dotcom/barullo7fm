#!/usr/bin/env python3
# Banco «CLÁSICOS» de Barullo 7FM: 32 sons deseñados de cero, coas técnicas de síntese FM de sempre,
# nas mesmas familias que un banco de fábrica dos 80 (pianos, metais, cordas, baixos, campás, efectos).
# Non parte dos valores de ningún banco existente: cada son está pensado aquí, operador a operador.
# Xera Resources/Barullo7FM-Clasicos.syx e Source/BancoClasicos.h (descricións).
import os

def op(ratio=1.0, ol=0, rates=(99, 99, 99, 99), levels=(99, 99, 99, 0), kvs=0, rs=0, ams=0,
       det=7, fixed=False, bp=39, ld=0, rd=0, lc=0, rc=0):
    """Un operador. ratio: 0.5, 1, 2… (con decimais = fino). fixed=True: ratio en Hz."""
    if fixed:
        import math
        e = max(0, min(3, int(math.floor(math.log10(ratio)))))
        m, fc, ff = 1, e, max(0, min(99, int(round((ratio / 10 ** e - 1) * 100))))
    else:
        if ratio < 1:
            fc, ff = 0, int(round((ratio / 0.5 - 1) * 100))
        else:
            fc = int(ratio); ff = int(round((ratio / fc - 1) * 100))
        m = 0; ff = max(0, min(99, ff))
    return dict(r=list(rates), l=list(levels), bp=bp, ld=ld, rd=rd, lc=lc, rc=rc, rs=rs, ams=ams, kvs=kvs,
                ol=ol, m=m, fc=fc, ff=ff, det=det)

def patch(name, alg, ops, fb=0, oks=1, lfo=(35, 0, 0, 0, 0, 0), pms=3, transp=24,
          pr=(99, 99, 99, 99), pl=(50, 50, 50, 50)):
    """ops = [OP1 … OP6]; lfo = (velocidade, retardo, PMD, AMD, sync, onda)."""
    assert len(ops) == 6 and len(name) <= 10, name
    b = []
    for o in reversed(ops):
        b += o['r'] + o['l'] + [o['bp'], o['ld'], o['rd'], o['lc'], o['rc'], o['rs'], o['ams'], o['kvs'],
                                o['ol'], o['m'], o['fc'], o['ff'], o['det']]
    b += list(pr) + list(pl) + [alg - 1, fb, oks] + list(lfo[:4]) + [lfo[4], lfo[5], pms, transp]
    b += [ord(c) for c in name.ljust(10)]
    assert len(b) == 155
    return b

OFF = op(ol=0)
P = []
def add(name, desc, *a, **k): P.append((name, patch(name, *a, **k), desc))

# ============================================================ teclados
add("PIANO ACUS", "Piano acústico: golpe do martelo, corpo de tres cordas e agudos que se apagan antes.", 5, [
    op(1, 99, (99, 38, 24, 62), (99, 78, 0, 0), kvs=2, rs=3, rd=18, rc=0),
    op(1, 82, (99, 40, 28, 64), (99, 72, 0, 0), kvs=5, rs=2),
    op(1, 93, (99, 36, 23, 62), (99, 76, 0, 0), kvs=2, rs=3, det=8, rd=18),
    op(3, 76, (99, 52, 32, 66), (99, 50, 0, 0), kvs=6, rs=2),
    op(2, 76, (99, 46, 28, 64), (99, 60, 0, 0), kvs=3, rs=4, det=6),
    op(7, 72, (99, 64, 40, 70), (99, 20, 0, 0), kvs=7, rs=3)], fb=3)
add("PIANO BRIL", "Piano brillante para pop: máis ataque e máis harmónicos ao tocar forte.", 5, [
    op(1, 99, (99, 34, 22, 60), (99, 80, 0, 0), kvs=3, rs=3, rd=14),
    op(1, 86, (99, 38, 26, 62), (99, 76, 0, 0), kvs=6, rs=2),
    op(1, 94, (99, 33, 22, 60), (99, 78, 0, 0), kvs=3, rs=3, det=9, rd=14),
    op(2, 80, (99, 48, 30, 64), (99, 56, 0, 0), kvs=6, rs=2),
    op(4, 72, (99, 44, 26, 62), (99, 52, 0, 0), kvs=3, rs=4, det=5),
    op(9, 74, (99, 60, 40, 70), (99, 24, 0, 0), kvs=7, rs=3)], fb=4)
add("P.ELEC 1", "Piano eléctrico de diapasón: o «ting» metálico aparece canto máis forte tocas.", 5, [
    op(1, 99, (99, 30, 22, 56), (99, 84, 0, 0), kvs=2, rs=3),
    op(1, 76, (99, 34, 24, 60), (99, 78, 0, 0), kvs=5, rs=2),
    op(1, 91, (99, 30, 21, 56), (99, 82, 0, 0), kvs=2, rs=3, det=9),
    op(14, 80, (99, 58, 34, 70), (99, 30, 0, 0), kvs=7, rs=2),
    op(1, 82, (99, 31, 22, 56), (99, 80, 0, 0), kvs=1, rs=3, det=5),
    op(1, 72, (99, 40, 28, 62), (99, 66, 0, 0), kvs=6, rs=2)], fb=5)
add("P.ELEC 2", "Piano eléctrico de lingüetas: suave a pouca forza e áspero cando lle pegas.", 5, [
    op(1, 99, (99, 34, 24, 58), (99, 82, 0, 0), kvs=3, rs=3, ams=1),
    op(1, 88, (99, 40, 28, 62), (99, 66, 0, 0), kvs=7, rs=2),
    op(1, 90, (99, 33, 24, 58), (99, 80, 0, 0), kvs=3, rs=3, det=8, ams=1),
    op(3, 74, (99, 52, 32, 66), (99, 40, 0, 0), kvs=7, rs=2),
    op(2, 70, (99, 40, 28, 58), (99, 70, 0, 0), kvs=2, rs=3, det=6),
    op(1, 76, (99, 46, 30, 62), (99, 56, 0, 0), kvs=6, rs=2)], fb=2, lfo=(42, 0, 0, 12, 0, 0))
add("CLAVINET", "Clavinet funky: pulsación curta, nasal e moi percusiva.", 3, [
    op(1, 99, (99, 44, 30, 80), (99, 80, 0, 0), kvs=3, rs=2),
    op(3, 86, (99, 48, 34, 80), (99, 72, 0, 0), kvs=5, rs=2),
    op(1, 82, (99, 56, 36, 80), (99, 60, 0, 0), kvs=6, rs=2),
    op(1, 92, (99, 44, 30, 80), (99, 80, 0, 0), kvs=3, rs=2, det=9),
    op(5, 84, (99, 50, 34, 80), (99, 66, 0, 0), kvs=5, rs=2),
    op(1, 80, (99, 60, 38, 80), (99, 50, 0, 0), kvs=6, rs=2)], fb=6)
add("CLAVECIN", "Clavicémbalo: punteo brillante de corda metálica, sen pedal e cun apagado seco.", 5, [
    op(1, 99, (99, 40, 26, 74), (99, 74, 0, 0), kvs=1, rs=3),
    op(3, 86, (99, 46, 30, 74), (99, 60, 0, 0), kvs=2, rs=3),
    op(2, 88, (99, 42, 27, 74), (99, 72, 0, 0), kvs=1, rs=3, det=8),
    op(5, 82, (99, 50, 32, 74), (99, 52, 0, 0), kvs=2, rs=3),
    op(4, 72, (99, 44, 28, 74), (99, 66, 0, 0), kvs=1, rs=3, det=6),
    op(7, 76, (99, 70, 40, 74), (99, 20, 0, 0), kvs=3, rs=4)], fb=3)
add("CELESTA", "Celesta: campaíñas de lámina, doces e con brillo de caixa de música.", 5, [
    op(1, 99, (99, 30, 24, 50), (99, 70, 0, 0), kvs=2, rs=3),
    op(4, 74, (99, 50, 34, 56), (99, 40, 0, 0), kvs=4, rs=3),
    op(2, 80, (99, 34, 26, 50), (99, 64, 0, 0), kvs=2, rs=3, det=8),
    op(7, 70, (99, 62, 40, 60), (99, 20, 0, 0), kvs=5, rs=4),
    op(4, 64, (99, 40, 30, 52), (99, 50, 0, 0), kvs=3, rs=4, det=6),
    op(1, 64, (99, 70, 44, 60), (99, 0, 0, 0), kvs=5)], fb=1)
add("ORGANO JAZ", "Órgano de rodas para jazz: cinco tiradores e o clic de percusión ao atacar.", 32, [
    op(0.5, 92, (99, 99, 99, 82), (99, 99, 99, 0), det=7),
    op(1, 99, (99, 99, 99, 82), (99, 99, 99, 0), det=8),
    op(1.5, 88, (99, 99, 99, 82), (99, 99, 99, 0), det=6),
    op(2, 95, (99, 99, 99, 82), (99, 99, 99, 0), det=7),
    op(4, 86, (99, 99, 99, 82), (99, 99, 99, 0), det=8),
    op(3, 99, (99, 62, 40, 82), (99, 0, 0, 0), kvs=2)], fb=0, lfo=(48, 0, 4, 0, 0, 4), pms=2)
add("ORG TUBOS", "Órgano de tubos: entra amodo, cheo de graves e cun sopro de aire nos tubos.", 19, [
    op(1, 99, (60, 99, 99, 46), (99, 99, 99, 0)),
    op(1, 70, (56, 99, 99, 46), (99, 99, 99, 0)),
    op(2, 52, (54, 99, 99, 46), (99, 99, 99, 0)),
    op(2, 88, (58, 99, 99, 46), (99, 99, 99, 0), det=8),
    op(4, 80, (56, 99, 99, 46), (99, 99, 99, 0), det=6),
    op(1, 44, (72, 60, 99, 46), (99, 70, 70, 0))], fb=5)
add("ACORDEON", "Acordeón de musette: tres lingüetas lixeiramente desafinadas que baten.", 5, [
    op(1, 99, (80, 99, 99, 70), (99, 99, 99, 0), det=7),
    op(1, 84, (78, 99, 99, 70), (99, 99, 99, 0), kvs=2),
    op(1, 94, (80, 99, 99, 70), (99, 99, 99, 0), det=4),
    op(2, 80, (78, 99, 99, 70), (99, 99, 99, 0), kvs=2),
    op(1, 92, (80, 99, 99, 70), (99, 99, 99, 0), det=10),
    op(1, 82, (78, 99, 99, 70), (99, 99, 99, 0), kvs=2)], fb=4)

# ============================================================ vento
add("METAIS 1", "Sección de metais: o brillo abrese co ataque, coma un sopro, e o ton sobe un chisco.", 22, [
    op(1, 99, (72, 44, 30, 60), (99, 94, 90, 0), kvs=1, rs=2),
    op(1, 88, (60, 40, 30, 60), (99, 88, 82, 0), kvs=4, rs=2),
    op(1, 92, (70, 44, 30, 60), (99, 94, 90, 0), kvs=1, rs=2, det=5),
    op(1, 92, (70, 44, 30, 60), (99, 94, 90, 0), kvs=1, rs=2, det=9),
    op(1, 90, (70, 44, 30, 60), (99, 94, 90, 0), kvs=1, rs=2, det=7),
    op(1, 84, (58, 40, 30, 60), (99, 86, 80, 0), kvs=4, rs=2)], fb=6,
    lfo=(34, 55, 8, 0, 0, 4), pms=3, pr=(84, 60, 99, 99), pl=(47, 50, 50, 50))
add("TROMPETA", "Trompeta: ataque cun pequeno ascenso de ton e vibrato que chega tarde.", 16, [
    op(1, 93, (76, 50, 34, 62), (99, 94, 90, 0), kvs=1, rs=2),
    op(1, 82, (66, 46, 32, 62), (99, 90, 86, 0), kvs=5, rs=2),
    op(1, 70, (66, 46, 32, 62), (99, 88, 80, 0), kvs=4),
    op(2, 60, (70, 50, 34, 62), (99, 80, 70, 0), kvs=3),
    op(1, 66, (62, 46, 32, 62), (99, 86, 80, 0), kvs=4),
    op(1, 62, (60, 46, 32, 62), (99, 80, 74, 0), kvs=2)], fb=6,
    lfo=(36, 62, 10, 0, 0, 4), pms=3, pr=(88, 60, 99, 99), pl=(46, 50, 50, 50))
add("FRAUTA", "Frauta traveseira: case senoidal, co aire do sopro só ao comezo.", 18, [
    op(1, 93, (64, 46, 99, 56), (99, 94, 94, 0), kvs=1),
    op(1, 56, (66, 46, 99, 56), (99, 90, 90, 0), kvs=3),
    op(5, 70, (99, 64, 50, 60), (99, 30, 0, 0), kvs=2),
    op(2, 40, (66, 46, 99, 56), (99, 86, 86, 0), kvs=2),
    op(1, 0), op(1, 0)], fb=7, lfo=(34, 50, 9, 0, 0, 4), pms=3)
add("CLARINETE", "Clarinete: harmónicos impares, son oco e redondo no grave.", 16, [
    op(1, 93, (70, 50, 99, 58), (99, 94, 94, 0), kvs=1),
    op(2, 82, (72, 50, 99, 58), (99, 90, 90, 0), kvs=4),
    op(2, 50, (72, 50, 99, 58), (99, 86, 86, 0), kvs=3),
    op(1, 0),
    op(3, 54, (74, 50, 99, 58), (99, 84, 84, 0), kvs=3),
    op(1, 0)], fb=0, lfo=(33, 60, 6, 0, 0, 4), pms=3)
add("OBOE", "Óboe: nasal e penetrante, cunha lingüeta dobre que se oe no ataque.", 18, [
    op(1, 93, (72, 50, 99, 58), (99, 94, 94, 0), kvs=1),
    op(1, 84, (74, 50, 99, 58), (99, 90, 90, 0), kvs=4),
    op(4, 58, (76, 50, 99, 58), (99, 84, 84, 0), kvs=3),
    op(3, 70, (74, 50, 99, 58), (99, 88, 88, 0), kvs=4),
    op(1, 0), op(1, 0)], fb=4, lfo=(35, 58, 7, 0, 0, 4), pms=3)

# ============================================================ cordas
add("CORDAS 1", "Sección de cordas: ataque lento, arcos lixeiramente desafinados e vibrato que entra amodo.", 1, [
    op(1, 99, (48, 40, 99, 42), (99, 96, 96, 0), kvs=1, rs=1, det=6),
    op(1, 80, (44, 40, 99, 42), (99, 92, 92, 0), kvs=3, rs=1),
    op(1, 97, (46, 40, 99, 42), (99, 96, 96, 0), kvs=1, rs=1, det=9),
    op(1, 76, (44, 40, 99, 42), (99, 90, 90, 0), kvs=3),
    op(3, 52, (40, 40, 99, 42), (99, 84, 84, 0), kvs=2),
    op(1, 50, (40, 40, 99, 42), (99, 80, 80, 0), kvs=2)], fb=4, lfo=(32, 50, 9, 0, 0, 4), pms=3)
add("ORQUESTRA", "Orquestra: cordas e metais xuntos para acordes grandes.", 5, [
    op(1, 99, (48, 40, 99, 44), (99, 96, 96, 0), kvs=1, det=6),
    op(1, 72, (44, 40, 99, 44), (99, 90, 90, 0), kvs=3),
    op(1, 92, (66, 44, 30, 54), (99, 92, 88, 0), kvs=2, det=8),
    op(1, 80, (56, 42, 30, 54), (99, 86, 80, 0), kvs=5),
    op(1, 96, (48, 40, 99, 44), (99, 96, 96, 0), kvs=1, det=9),
    op(1, 70, (44, 40, 99, 44), (99, 88, 88, 0), kvs=3)], fb=5, lfo=(33, 52, 8, 0, 0, 4), pms=3)
add("COROS", "Coro de voces «aah»: formantes suaves, ataque lento e vibrato.", 22, [
    op(1, 99, (46, 40, 99, 44), (99, 96, 96, 0), kvs=1),
    op(1, 62, (44, 40, 99, 44), (99, 92, 92, 0), kvs=2),
    op(2, 84, (46, 40, 99, 44), (99, 94, 94, 0), kvs=1, det=8),
    op(3, 78, (46, 40, 99, 44), (99, 92, 92, 0), kvs=1, det=6),
    op(5, 66, (46, 40, 99, 44), (99, 90, 90, 0), kvs=1, det=8),
    op(1, 50, (44, 40, 99, 44), (99, 88, 88, 0), kvs=1)], fb=2, lfo=(30, 40, 10, 0, 0, 4), pms=3)

# ============================================================ corda pulsada
add("GUITARRA", "Guitarra de nailon: punteo cálido co ruído da uña ao comezo.", 5, [
    op(1, 99, (99, 44, 26, 62), (99, 72, 0, 0), kvs=2, rs=3),
    op(3, 80, (99, 46, 30, 64), (99, 56, 0, 0), kvs=4, rs=2),
    op(1, 90, (99, 42, 26, 62), (99, 74, 0, 0), kvs=2, rs=3, det=8),
    op(1, 82, (99, 40, 28, 62), (99, 66, 0, 0), kvs=4, rs=2),
    op(2, 72, (99, 56, 34, 62), (99, 40, 0, 0), kvs=3, rs=3),
    op(5, 74, (99, 66, 44, 70), (99, 10, 0, 0), kvs=5, rs=3)], fb=3)
add("CORDA XAP", "Corda pulsada xaponesa: ataque metálico, afinación tensa e caída rápida.", 3, [
    op(1, 99, (99, 52, 32, 66), (99, 64, 0, 0), kvs=2, rs=3),
    op(3, 92, (99, 46, 32, 66), (99, 60, 0, 0), kvs=4, rs=2),
    op(1, 82, (99, 58, 38, 70), (99, 40, 0, 0), kvs=5, rs=2),
    op(1, 92, (99, 50, 32, 66), (99, 66, 0, 0), kvs=2, rs=3, det=9),
    op(4, 86, (99, 48, 32, 66), (99, 56, 0, 0), kvs=3, rs=2),
    op(7, 80, (99, 66, 44, 70), (99, 10, 0, 0), kvs=5, rs=3)], fb=5)
add("ARPA", "Arpa: punteo redondo e limpo que vai morrendo amodo.", 5, [
    op(1, 99, (99, 32, 24, 46), (99, 70, 0, 0), kvs=2, rs=3),
    op(1, 76, (99, 40, 30, 50), (99, 60, 0, 0), kvs=4, rs=2),
    op(2, 76, (99, 36, 26, 46), (99, 62, 0, 0), kvs=2, rs=3, det=8),
    op(2, 70, (99, 46, 32, 50), (99, 50, 0, 0), kvs=4, rs=2),
    op(1, 84, (99, 34, 25, 46), (99, 66, 0, 0), kvs=2, rs=3, det=6),
    op(3, 68, (99, 56, 36, 54), (99, 20, 0, 0), kvs=5, rs=2)], fb=1)

# ============================================================ baixos
add("BAIXO ELEC", "Baixo eléctrico FM: golpe redondo, punteo marcado e corpo que aguanta.", 16, [
    op(1, 99, (99, 46, 28, 70), (99, 88, 72, 0), kvs=2, rs=2),
    op(1, 88, (99, 44, 30, 70), (99, 74, 60, 0), kvs=5, rs=1),
    op(1, 80, (99, 50, 32, 70), (99, 60, 40, 0), kvs=5, rs=1),
    op(3, 78, (99, 60, 40, 70), (99, 20, 0, 0), kvs=6, rs=1),
    op(0.5, 80, (99, 50, 30, 70), (99, 70, 60, 0), kvs=3),
    op(1, 70, (99, 60, 36, 70), (99, 40, 20, 0), kvs=4)], fb=6)
add("BAIXO SINT", "Baixo de sintetizador: dente de serra cun filtro que se pecha só.", 16, [
    op(0.5, 93, (99, 40, 99, 72), (99, 92, 92, 0), kvs=1),
    op(0.5, 94, (99, 50, 34, 72), (99, 62, 50, 0), kvs=5),
    op(1, 0),
    op(1, 0),
    op(0.5, 88, (99, 46, 30, 72), (99, 70, 60, 0), kvs=4),
    op(0.5, 86, (99, 99, 99, 72), (99, 99, 99, 0))], fb=7)
add("BAIXO PUNT", "Contrabaixo punteado: dedo sobre corda grosa, cálido e curto.", 16, [
    op(1, 99, (99, 50, 30, 64), (99, 80, 0, 0), kvs=2, rs=2),
    op(1, 84, (99, 46, 30, 64), (99, 66, 0, 0), kvs=5, rs=1),
    op(2, 74, (99, 56, 36, 64), (99, 40, 0, 0), kvs=5, rs=1),
    op(1, 0),
    op(1, 76, (99, 56, 32, 64), (99, 50, 0, 0), kvs=3),
    op(4, 68, (99, 70, 48, 70), (99, 0, 0, 0), kvs=5)], fb=2)

# ============================================================ láminas e percusión
add("VIBRAFONO", "Vibráfono: lámina metálica cun motor que fai trémolo, e golpe suave de baqueta.", 5, [
    op(1, 99, (99, 26, 24, 44), (99, 60, 0, 0), kvs=3, rs=3, ams=2),
    op(4, 78, (99, 50, 34, 50), (99, 30, 0, 0), kvs=5, rs=2),
    op(1, 80, (99, 28, 24, 44), (99, 58, 0, 0), kvs=3, rs=3, det=8, ams=2),
    op(1, 50, (99, 50, 36, 48), (99, 30, 0, 0), kvs=4),
    op(10, 72, (99, 62, 44, 56), (99, 0, 0, 0), kvs=5, rs=3),
    op(1, 60, (99, 80, 60, 60), (99, 0, 0, 0), kvs=6)], fb=0, lfo=(38, 0, 0, 22, 0, 0))
add("MARIMBA", "Marimba: madeira golpeada, nota curta e grave chea.", 5, [
    op(1, 99, (99, 44, 36, 56), (99, 46, 0, 0), kvs=3, rs=3),
    op(4, 78, (99, 58, 40, 60), (99, 10, 0, 0), kvs=5, rs=2),
    op(4, 60, (99, 66, 46, 60), (99, 0, 0, 0), kvs=3, rs=4),
    op(1, 0),
    op(1, 70, (99, 48, 38, 56), (99, 40, 0, 0), kvs=3, rs=3, det=8),
    op(10, 70, (99, 72, 56, 66), (99, 0, 0, 0), kvs=6)], fb=0)
add("CAMPAS TUB", "Campás tubulares: golpe metálico, parciais inharmónicos e unha cola longa.", 5, [
    op(1, 99, (99, 22, 18, 30), (99, 60, 0, 0), kvs=2, rs=2),
    op(3.51, 76, (99, 34, 22, 34), (99, 50, 0, 0), kvs=4, rs=2),
    op(2.0, 84, (99, 26, 20, 30), (99, 56, 0, 0), kvs=2, rs=2, det=9),
    op(1.41, 70, (99, 40, 24, 34), (99, 40, 0, 0), kvs=4, rs=2),
    op(3.0, 72, (99, 30, 22, 30), (99, 50, 0, 0), kvs=2, rs=2, det=4),
    op(5.19, 62, (99, 56, 32, 40), (99, 0, 0, 0), kvs=5)], fb=0)
add("BIDON", "Bidón de aceiro do Caribe: nota metálica e redonda cun «bong» ao comezo.", 5, [
    op(1, 99, (99, 40, 28, 52), (99, 56, 0, 0), kvs=3, rs=3),
    op(2.72, 82, (99, 46, 32, 56), (99, 50, 0, 0), kvs=4, rs=2),
    op(2, 76, (99, 44, 30, 52), (99, 52, 0, 0), kvs=3, rs=3, det=8),
    op(1, 70, (99, 50, 34, 56), (99, 40, 0, 0), kvs=4),
    op(4.0, 54, (99, 50, 34, 54), (99, 30, 0, 0), kvs=3),
    op(1, 70, (99, 62, 44, 60), (99, 10, 0, 0), kvs=5)], fb=2, pr=(99, 70, 99, 99), pl=(54, 50, 50, 50))
add("TIMBAL", "Timbal de orquestra: golpe de pel grave, o ton baixa un pouco e retumba.", 16, [
    op(1, 93, (99, 30, 22, 36), (99, 64, 0, 0), kvs=3, rs=1),
    op(1.5, 70, (99, 44, 28, 40), (99, 40, 0, 0), kvs=4),
    op(2.3, 60, (99, 56, 34, 44), (99, 20, 0, 0), kvs=4),
    op(1, 0),
    op(0.5, 62, (99, 40, 26, 40), (99, 50, 0, 0), kvs=3),
    op(3.7, 60, (99, 84, 60, 60), (99, 0, 0, 0), kvs=6)], fb=4, transp=12,
    pr=(99, 62, 99, 99), pl=(56, 50, 50, 50))

# ============================================================ sintes e efectos
add("SINT SOLO", "Solo de sintetizador: dente de serra brillante, entrada co ton desde abaixo e vibrato.", 18, [
    op(1, 93, (99, 50, 99, 66), (99, 96, 96, 0), kvs=1),
    op(1, 90, (99, 44, 99, 66), (99, 90, 90, 0), kvs=3),
    op(1, 78, (99, 99, 99, 66), (99, 99, 99, 0)),
    op(2, 66, (99, 50, 99, 66), (99, 84, 84, 0), kvs=3, det=9),
    op(1, 0), op(1, 0)], fb=7, lfo=(37, 46, 10, 0, 0, 4), pms=3,
    pr=(90, 99, 99, 99), pl=(46, 50, 50, 50))
add("ASUBIO", "Asubío: un ton puro que sobe ao entrar, co aire dos beizos.", 18, [
    op(2, 93, (70, 99, 99, 66), (99, 99, 99, 0)),
    op(1, 0),
    op(9, 40, (99, 64, 99, 66), (99, 40, 40, 0)),
    op(1, 0), op(1, 0), op(1, 0)], fb=7, lfo=(40, 30, 12, 0, 0, 4), pms=4,
    pr=(80, 99, 99, 99), pl=(42, 50, 50, 50))
add("DESPEGUE", "Despegue: un motor de reacción que arrinca, sobe e se vai.", 1, [
    op(1, 0),
    op(1, 0),
    op(1, 99, (40, 99, 30, 30), (99, 99, 0, 0)),
    op(1, 84, (40, 99, 30, 30), (99, 99, 0, 0)),
    op(3, 90, (40, 99, 30, 30), (99, 99, 0, 0)),
    op(7, 99, (99, 99, 99, 30), (99, 99, 99, 0))], fb=7,
    pr=(30, 20, 99, 30), pl=(20, 99, 99, 0))

assert len(P) == 32, len(P)

def pack(v):
    o = []
    for k in range(6):
        s = v[k*21:(k+1)*21]
        o += s[0:11] + [(s[12] << 2) | s[11], (s[20] << 3) | s[13], (s[15] << 2) | s[14], s[16], (s[18] << 1) | s[17], s[19]]
    o += v[126:134] + [v[134], (v[136] << 3) | v[135]] + v[137:141] + [(v[143] << 4) | (v[142] << 1) | v[141], v[144]] + v[145:155]
    assert len(o) == 128
    return o

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
body = sum((pack(b) for _, b, _ in P), [])
syx = bytes([0xF0, 0x43, 0x00, 0x09, 0x20, 0x00] + body + [(128 - (sum(body) & 127)) & 127, 0xF7])
open(os.path.join(root, "Resources", "Barullo7FM-Clasicos.syx"), "wb").write(syx)
def esc(s):
    return '"' + ''.join(c if ord(c) < 128 else ''.join('\\x%02x' % b for b in c.encode()) + '""' for c in s) + '"'
lines = ["// Xerado por tools/clasicos.py. Non editar á man.", "#pragma once", "namespace b7 {",
         "static const char* const CLASICOS_DESC[32] = {"]
lines += ["  %s," % esc(d) for _, _, d in P]
lines += ["};", "}"]
open(os.path.join(root, "Source", "BancoClasicos.h"), "w").write("\n".join(lines) + "\n")
print("32 sons:", ", ".join(n for n, _, _ in P))
