#!/bin/bash
# As probas de Barullo 7FM na CI común (escoita-nucleo): chámase desde a raíz con
# mac, windows ou linux, coa compilación en build/
set -eo pipefail
EXE=""; [ "$1" = windows ] && EXE=".exe"
./build/B7RenderTest_artefacts/Release/B7RenderTest$EXE prueba.wav
