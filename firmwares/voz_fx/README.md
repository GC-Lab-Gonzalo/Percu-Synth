# voz_fx — procesador de voz en tiempo real

Hablas o cantas al micrófono INMP441 y la voz sale transformada por el DAC, sin grabar nada
(~12 ms de latencia). BTN1 cambia de efecto, POT1–POT3 lo modifican en vivo y POT4 es siempre
el volumen.

**Usa audífonos**: con parlante, el micrófono escucha la salida y el eco, la catedral y el
flanger se acoplan.

## Botones

| Botón | Qué hace |
|---|---|
| BTN1 | Efecto siguiente (1 → 11 → 1) |
| BTN2 | Puerta de ruido on/off (apagada = también pasa el sonido ambiente; LED 0 en blanco) |
| BTN3–5 | Libres |

## Efectos (POT4 = volumen, siempre)

| # | Efecto | POT1 | POT2 | POT3 | LED |
|---|---|---|---|---|---|
| 1 | VOZ (limpia, alta calidad) | graves ±6 dB | agudos ±6 dB | compresión (0 = nada) | rojo |
| 2 | ELECTRO (cadena pro + trémolo al tempo) | tempo 70–180 BPM | división: 1 · 1/2 · 1/4 · 1/8 · 1/16 · 1/32 | profundidad del trémolo | naranjo (late con el trémolo) |
| 3 | AUTOTUNE + vocoder (escala fija Si♭ menor, por ahora) | mezcla autotune ↔ vocoder | octava abajo del vocoder | velocidad (0 natural → 1 robot) | amarillo |
| 4 | ROBOT (modulador en anillo) | frecuencia 30 Hz–1 kHz | mezcla | brillo | lima |
| 5 | VOCODER (la voz sintetizada, 12 bandas) | nota Do2–Do4 | acorde (6) | brillo 0.8–8 kHz | verde |
| 6 | PITCH | semitonos −12..+12 | mezcla | grano 25–85 ms | verde agua |
| 7 | ARMONIZADOR | acorde (6) | nivel de las voces | ancho estéreo | celeste |
| 8 | ECO (cinta) | tiempo 60–720 ms | realimentación | brillo | azul claro |
| 9 | CATEDRAL (reverb) | tamaño | brillo | mezcla | azul |
| 10 | RADIO | centro de banda | estrechez | saturación | violeta |
| 11 | FLANGER | velocidad | profundidad | realimentación | rosado |

AUTOTUNE: la escala está fija en Si♭ menor (la de *Instant Crush*; `atTono`/`atEscala` en el
código). La capa de vocoder es un sinte que toca la nota ya corregida (+ la octava de abajo con
POT2), analizado con la entrada retrasada lo mismo que el PSOLA para que ambos queden en el mismo
tiempo; la mezcla es de potencia constante. Detecta la altura con YIN (a 11 kHz, cada ~12 ms, 100–790 Hz) y la corrige con
PSOLA sincronizado al período: la fuente avanza de a períodos enteros, así los granos se
suman en fase y no aparece el chorus del desplazador de dos cabezas. Latencia ~26 ms. En
simulación, con velocidad al máximo, una voz en glissando (mediana 25 cents fuera de
semitono) sale a 2 cents de un semitono exacto.

En VOZ, con los tres pots al centro la voz sale plana (graves y agudos tienen zona muerta al
centro). ELECTRO lleva una cadena fija: presencia +3 dB en 3 kHz → compresor 4:1 → doblador
estéreo (12/19 ms) + delay ping-pong a 1/8 del tempo (1/16 bajo ~88 BPM) + reverb plate, y el
trémolo encima de todo. La fase del trémolo se cuenta en compases, así que al cambiar de
división sigue cayendo en el tiempo.

LEDs 1–5 = vúmetro. El monitor serie (115200) imprime el efecto y qué hace cada pot al cambiarlo.

## Cableado del micrófono

INMP441: WS → GPIO 11 · SCK → GPIO 12 · SD → GPIO 13 · L/R → GND · **VDD → 3.3 V, nunca 5 V**
(a 5 V el micro trabaja fuera de especificación: datos corruptos, chasquidos). Para comprobar
el micro sin efectos, usa el efecto 1 (VOZ) con POT1 y POT2 al centro y POT3 al mínimo: sin EQ ni compresión.

## Decisiones

- **El micro corre a 44.1 kHz, igual que el DAC**, con la misma fuente de reloj: no derivan y
  no hay que remuestrear. Leer el micro marca el ritmo de la tarea de audio (core 1); botones,
  pots y LEDs van en otra tarea en el core 0.
- Cadena fija: pasa-altos 90 Hz → ganancia (+18 dB, medida pasando el micro directo al DAC) → **puerta de
  ruido** → efecto → volumen → bloqueador DC → limitador con lookahead de 64 muestras →
  pasa-bajos 13 kHz. La puerta va antes del efecto, así las colas del eco y la reverb no se cortan.
- Los retardos comparten **un solo bloque de ~203 KB** (los efectos no suenan a la vez); se
  limpia al cambiar, durante un fundido de 5 ms, así el cambio no hace clic.
- Ajustes en constantes al inicio del `.ino`: `GANANCIA_MIC`, `UMBRAL_PUERTA` (súbelo si la
  sala es ruidosa y la puerta se queda abierta), `MASTER`.
- Verificado en simulación de PC con voz sintética normal y gritada, los 11 efectos con los
  pots en 0, 0.5, 1 y al azar: 0 NaN, 0 muestras recortadas, salida en cero con el micro en
  silencio.
