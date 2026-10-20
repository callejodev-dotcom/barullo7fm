#!/bin/sh
# Compila e executa a proba do motor (desde a raíz do proxecto)
S=tools/msfa
clang++ -std=c++17 -O2 -ISource -Itools -o ${OUT:-./enginetest} tools/enginetest.cpp \
  $S/dx7note.cc $S/env.cc $S/exp2.cc $S/fm_core.cc $S/fm_op_kernel.cc $S/freqlut.cc $S/lfo.cc $S/pitchenv.cc $S/porta.cpp $S/sin.cc && ${OUT:-./enginetest}
