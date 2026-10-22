**Barullo 7FM** é un sintetizador FM de 6 operadores, 32 algoritmos e 16 voces, compatible cos sons do DX7: le e garda os seus ficheiros .syx e recibe os volcados e os cambios de parámetro que manda o teclado polo MIDI. Sen mostras e sen latencia.

O motor emula a máquina chip a chip: o programa da CPU (coas súas táboas e redondeos), o xerador de envolventes e o chip de son (táboas de log-seno e exponencial, 23 bits de fase, saída de 15 bits), o conversor de 12 bits por voz e o filtro analóxico de saída.

Unha ferramenta de [Escoita](https://www.escoita.link).

## Descargas

| Sistema | Instalador | Á man |
|---|---|---|
| **Mac** (Apple Silicon e Intel, macOS 11+): AU, VST3 e app | `Barullo7FM-…-mac.pkg` | `Barullo7FM-…-mac.zip` |
| **Windows** (64 bits): VST3 e app | `Barullo7FM-…-windows-setup.exe` | `Barullo7FM-…-windows-x64.zip` |
| **Linux** (x86_64): VST3, LV2 e app | — | `Barullo7FM-…-linux-x86_64.tar.gz` → `./instalar.sh` |

Se o sistema avisa ao abrilo:
- **Mac:** clic dereito sobre o instalador → *Abrir* → *Abrir*.
- **Windows:** «Windows protexeu o teu PC» → *Máis información* → *Executar de todos os xeitos*.

Avisan porque os instaladores non levan sinatura de pago, non porque haxa nada raro.

## Que ten

- **Sons**: dous bancos propios dentro do plugin, de 32 sons cada un, deseñados de cero para Barullo 7FM:
  - **CLASICOS**: as familias dun banco de fábrica dos 80 (pianos, pianos eléctricos, órganos, metais, cordas, vento, corda pulsada, baixos, láminas, campás, efectos);
  - **BARULLO**: os primeiros sons de Escoita.
- **Carpeta de cartuchos**: escolle unha carpeta e todos os bancos `.syx` que haxa nela aparecen na lista. Barullo recoñece os bancos de fábrica (ROM1A–ROM4B, VRC101–VRC112) e amósaos co seu nome e descrición; se tes un DX7, podes volcar a súa memoria (FUNCTION, botón 8 ata «MIDI TRANSMIT ?», YES) e gardala alí. Hai miles de bancos para DX7 compartidos libremente polos seus autores: todos funcionan.
- **Memoria interna** de 32 sons que se pode gravar (gárdase co proxecto), e **abrir** calquera `.syx` de 32 sons ou dun só son.
- **Editor completo**: os seis operadores (frecuencia en razón ou fixa, desafinación, envolvente de 4 taxas e 4 niveis, nivel, sensibilidade á velocidade e á AM, escala de teclado e de taxa), o algoritmo (cos 32 debuxados), a realimentación, o LFO, a envolvente de ton, a transposición e o nome.
- **MIDI**:
  - notas, pedal (CC64), bend, roda de modulación (CC1), sopro (CC2), pedal de expresión (CC4) e aftertouch;
  - cambio de programa = memoria 1–32;
  - **SysEx**: un banco recibido vai á memoria interna; un son, ao búfer de edición. Os cambios de parámetro que manda o teclado ao mover o deslizador de datos aplícanse ao momento.
- **Función**: alcance do bend e da roda de modulación, e a que se aplica a roda (ton, amplitude, envolvente).
- **Presets propios, A/B e desfacer** (do núcleo común, na función):
  - **PRESETS**: garda e carga todo (o son coas túas edicións, a memoria interna, o `.syx` aberto e a función) en ficheiros `.b7preset`, en Documentos/Barullo 7FM/Presets; no mesmo menú, abrir a carpeta e copiar entre A e B;
  - **A/B**: dous xogos de axustes para comparar; cambiar de lado garda o actual;
  - **desfacer e refacer** (as frechas): os parámetros do son, o algoritmo, o nome, os operadores, o son inicial, escoller son ou cartucho, gardar na interna, abrir un `.syx` e os presets e o A/B. Cada clic é un paso.
- **Saída**: mono (como o orixinal) en estéreo, volume e limitador de seguridade a −1 dBFS.
- **Versións novas**: unha vez ao día, pregúntase a GitHub cal é a última versión (sen mandar ningún dato) e, se hai unha nova, avísase debaixo de ESCOITA. cunha ligazón ás descargas. Co clic dereito en ESCOITA. vese a versión que tes, ábrense as descargas e desactívase o aviso. En Linux, de momento, non avisa (JUCE vai sen curl).

## Estrutura

| Ficheiro | Que é |
|---|---|
| `Source/dx7/Engine.h` | A máquina: CPU (firmware), EGS (envolventes e frecuencias) e OPS (operadores), a 49 096 Hz |
| `Source/dx7/Rom.h` | As táboas do programa da CPU (v1.8) |
| `Source/dx7/Algorithms.h` | Os 32 algoritmos |
| `Source/dx7/Output.h` | Conversor, mostraxe e retención, filtro de 16 kHz e paso á frecuencia do DAW |
| `Source/Synth7.h` | A máquina vista desde o plugin |
| `Source/Dx7Patch.h` | Formatos dos sons: VCED (155 bytes) e bancos VMEM de 32, SysEx |
| `Source/Algo.h` | Debuxo dos 32 algoritmos, deducido da propia táboa do motor |
| `Source/Cartuchos.h` | Os bancos que van dentro e a táboa de bancos coñecidos (só pegadas e nomes; xerado por `tools/cartuchos.py`) |
| `Source/BancoClasicos.h`, `Source/Banco.h` | Descricións dos sons propios (xerados por `tools/clasicos.py` e `tools/banco.py`) |
| `Source/Plugin*.cpp` | O plugin JUCE e a interface |
| `packaging/` | Os datos do produto (`produto.env`), os LEEME dos instaladores e as notas da versión; os instaladores e a CI son do núcleo común, [escoita-nucleo](https://github.com/callejodev-dotcom/escoita-nucleo) (versión fixada en `CMakeLists.txt`, `ESCOITA_NUCLEO_VERSION`, e en `.github/workflows/build.yml`) |
| `tools/probas-ci.sh` | As probas que pasa a CI en cada sistema |
| `tools/RenderTest.cpp` | Proba sen interface |
| `tools/enginetest.cpp` | Compara o motor co anterior (msfa, en `tools/msfa/`): afinación, niveis, espectros |

## Probas

- **Afinación**: o La4 dá 440,13 Hz, coma no orixinal (a súa táboa de notas ten resolución de 1/1024 de oitava); todo o teclado queda a +0,1 cents.
- **Os 32 algoritmos** estables e debuxados igual que os calcula o motor.
- **Bancos**: ler e volver gardar un .syx dá exactamente os mesmos bytes.
- **Todos os sons** (os propios e, con `B7_CARTUCHOS=carpeta`, os da carpeta) tocados un por un (nota, acorde, bend e roda): ningún valor non numérico; a metade queda por baixo de −12 dBFS.
- **Bloques**: con bloques de 512 e de 37 mostras o resultado é o mesmo.
- **SysEx**: banco e cambio de parámetro recibidos polo MIDI.
- **Estado**: memoria interna, cartucho aberto, son editado e tamaño da xanela gárdanse e recupéranse iguais.
- **A/B, presets propios e desfacer**: cada lado garda o seu son editado, a súa memoria e os seus parámetros; un preset gardado cárgase igual; desfanse e refanse un parámetro do son e un cambio de son.
- **Pegada da saída**: FNV-1a de 64 bits das mostras dun render fixo (ROM1A e un forte co limitador traballando), para comprobar que un cambio de organización non cambia nin un bit do son.
- **Validación**: pluginval ao nivel máximo (10), con probas de interface; auval no Mac.

## Publicar unha versión

```
git tag v0.2.0 && git push origin v0.2.0
```

A versión sae da liña `project(Barullo7FM VERSION …)` de `CMakeLists.txt`.

## Créditos e licenzas

- **Barullo 7FM é software libre**, baixo a licenza **GNU Affero General Public License v3.0** (ficheiro `LICENSE`): podes usalo, estudalo, modificalo e compartilo; se distribúes unha versión modificada, ten que ir co seu código e coa mesma licenza. Usa JUCE 8 coa súa licenza AGPLv3.
- O chip de son, segundo a análise do silicio de Ken Shirriff (righto.com, 2021–2022).
- O programa da CPU, segundo o desensamblado comentado de ajxs (github.com/ajxs/yamaha_dx7_rom_disassembly).
- O modelo das envolventes e da desafinación, medido por *Music Synthesizer for Android* (msfa), © Google Inc., coas melloras de Pascal Gauthier e Jean Pierre Cimalando (licenza Apache 2.0; o código orixinal só se usa nas probas, en `tools/msfa/`).
- Tipografía Barlow (licenza OFL, `Resources/Barlow-OFL.txt`).
- Letra da pantalla LCD: glcdfont de Adafruit (licenza BSD, `Resources/LICENSE-Adafruit-GFX.txt`).
