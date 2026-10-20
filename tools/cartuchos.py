#!/usr/bin/env python3
# Xera Source/Cartuchos.h:
#  · os bancos que van dentro do plugin (só os nosos: CLASICOS e BARULLO);
#  · a táboa de bancos coñecidos: só a pegada de cada banco (tools/bancos_conhecidos.txt, calculada por
#    tools/hashbanks.cpp), o nome e unha descrición. Ningún dato dos sons: se alguén ten eses bancos
#    na súa carpeta de cartuchos, Barullo recoñéceos e amósaos co seu nome.
import os
here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(here)

KNOWN = {  # ficheiro → (nome, descrición en galego)
 'rom1a': ('ROM1A', 'Mestre (Europa e Xapón)'),
 'rom1b': ('ROM1B', 'Teclados e corda pulsada'),
 'rom2a': ('ROM2A', 'Orquestra e percusión'),
 'rom2b': ('ROM2B', 'Sintes, mesturas e efectos'),
 'rom3a': ('ROM3A', 'Mestre (EUA)'),
 'rom3b': ('ROM3B', 'Teclados e corda pulsada'),
 'rom4a': ('ROM4A', 'Orquestra e percusión'),
 'rom4b': ('ROM4B', 'Sintes, mesturas e efectos'),
 'vrc101a': ('VRC101A', 'Teclados, corda pulsada e percusión afinada'),
 'vrc101b': ('VRC101B', 'Teclados, corda pulsada e percusión afinada'),
 'vrc102a': ('VRC102A', 'Instrumentos de vento'),
 'vrc102b': ('VRC102B', 'Instrumentos de vento'),
 'vrc103a': ('VRC103A', 'Sons sostidos'),
 'vrc103b': ('VRC103B', 'Sons sostidos'),
 'vrc104a': ('VRC104A', 'Percusión'),
 'vrc104b': ('VRC104B', 'Percusión'),
 'vrc105a': ('VRC105A', 'Efectos de son'),
 'vrc105b': ('VRC105B', 'Efectos de son'),
 'vrc106a': ('VRC106A', 'Sintetizador'),
 'vrc106b': ('VRC106B', 'Sintetizador'),
 'vrc107a': ('VRC107A', 'Selección de David Bristow'),
 'vrc107b': ('VRC107B', 'Selección de David Bristow'),
 'vrc108a': ('VRC108A', 'Selección de Gary Leuenberger'),
 'vrc108b': ('VRC108B', 'Selección de Gary Leuenberger'),
 'vrc109a': ('VRC109A', 'Studio 64'),
 'vrc109b': ('VRC109B', 'Studio 64'),
 'vrc110a': ('VRC110A', 'Selección de Bo Tomlyn'),
 'vrc110b': ('VRC110B', 'Selección de Bo Tomlyn'),
 'vrc111a': ('VRC111A', 'Selección de Bo Tomlyn II'),
 'vrc111b': ('VRC111B', 'Selección de Bo Tomlyn II'),
 'vrc112a': ('VRC112A', 'Live 64 (Akira Inoue)'),
 'vrc112b': ('VRC112B', 'Live 64 (Akira Inoue)'),
}
ORDER = list(KNOWN.keys())

def load(path):
    d = open(path, 'rb').read()
    assert len(d) >= 4104 and d[0] == 0xF0 and d[1] == 0x43 and d[3] == 9, path
    body = d[6:6 + 4096]
    assert (128 - (sum(body) & 127)) & 127 == d[4102], 'checksum ' + path
    return body

def esc(s):
    return ''.join(c if ord(c) < 128 and c not in '"\\' else ('\\"' if c == '"' else '\\\\' if c == '\\' else ''.join('\\x%02x""' % b for b in c.encode())) for c in s)

hashes = {}
for line in open(os.path.join(here, 'bancos_conhecidos.txt')):
    line = line.strip()
    if not line or line.startswith('#'): continue
    stem, h = line.split()
    hashes[stem] = h

embedded = [(load(os.path.join(root, 'Resources', 'Barullo7FM-Clasicos.syx')), 'CLASICOS', 'Sons clásicos de Escoita', 1),
            (load(os.path.join(root, 'Resources', 'Barullo7FM-Banco.syx')), 'BARULLO', 'Sons propios de Escoita', 2)]
o = ['// Xerado por tools/cartuchos.py. Non editar á man.',
     '#pragma once', '#include <cstdint>', 'namespace b7 {',
     '// bancos que van dentro do plugin (VMEM, 128 bytes por son); descKind: 1 CLASICOS, 2 BARULLO',
     'struct Cartridge { const char* name; const char* title; const uint8_t* vmem; int descKind; };',
     'constexpr int NUM_CARTRIDGES = %d;' % len(embedded)]
for i, (b, n, t, k) in enumerate(embedded):
    o.append('static const uint8_t CART_%d[4096] = {' % i)
    for r in range(0, 4096, 32):
        o.append('  ' + ','.join(str(x) for x in b[r:r + 32]) + ',')
    o.append('};')
o.append('static const Cartridge CARTRIDGES[NUM_CARTRIDGES] = {')
for i, (b, n, t, k) in enumerate(embedded):
    o.append('  { "%s", "%s", CART_%d, %d },' % (n, esc(t), i, k))
o.append('};')
o += ['// bancos coñecidos: só a pegada (bankHash), o nome e a descrición; a orde é a da lista',
      'struct KnownBank { uint64_t hash; const char* name; const char* title; };']
rows = [(hashes[s], KNOWN[s][0], KNOWN[s][1]) for s in ORDER if s in hashes]
o.append('constexpr int NUM_KNOWN_BANKS = %d;' % len(rows))
o.append('static const KnownBank KNOWN_BANKS[NUM_KNOWN_BANKS] = {')
for h, n, t in rows:
    o.append('  { %s, "%s", "%s" },' % (h, n, esc(t)))
o += ['};', '} // namespace b7', '']
open(os.path.join(root, 'Source', 'Cartuchos.h'), 'w').write('\n'.join(o))
print('dentro:', len(embedded), ' coñecidos:', len(rows))
