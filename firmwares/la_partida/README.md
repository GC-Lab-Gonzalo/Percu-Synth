# la_partida — "La partida" de Víctor Jara en 3 pistas MIDI

El PercuSynth toca el arreglo de **"La partida"** (Víctor Jara) desde sus tres pistas MIDI,
cada una con su propio motor de síntesis, a los tiempos exactos del archivo.

| Pista | Motor |
|---|---|
| **Melodía** (57 notas) | **Lead**: dos sierras PolyBLEP desafinadas ±7 cents + cuadrada → filtro pasa-bajos resonante (SVF) con envolvente de filtro. Glide corto entre notas ligadas y vibrato que entra ~180 ms después del ataque |
| **Bajo** (18 notas) | Una octava abajo del archivo (`TRANSP_BAJO = -12`), y las notas que igual quedan sobre La2 bajan otra octava (`BAJO_NOTA_MAX`): toda la línea vive en La1–La2. **Sierra + cuadrada** con un leve desafine entre ellas (−4 / +4 cents): el batido lento llena de armónicos. Saturación suave → SVF pasa-bajos con un toque de envolvente de filtro |
| **Arpegio** (127 notas) | **Pluck electrónico**: sierra + pulso 25 % desafinados 6 cents, cada nota con su propio filtro pasa-bajos que se abre de golpe (3.8 octavas) y se cierra 4× más rápido que el volumen. El piso del filtro sigue a la nota (1.8 × la fundamental), así que las colas quedan oscuras y afinadas. Paneo fijo por altura |

## Controles

| Botón | Panel | POT1 | POT2 | POT3 | POT4 |
|---|---|---|---|---|---|
| **BTN1** | **Play / Stop** + panel **general** | volumen general | velocidad (×0.5 … ×2) | — | — |
| **BTN2** | **melodía** | volumen | vibrato | filtro LPF | resonancia |
| **BTN3** | **bajo** | volumen | filtro LPF | resonancia | — |
| **BTN4** | **arpegio** | volumen | ataque (3–300 ms) | decay (0.08–1.2 s) | — |

- Play parte siempre desde el comienzo. Al terminar, la canción se detiene sola y deja sonar
  las colas (`REPETIR_CANCION = true` para que vuelva a empezar).
- **Velocidad:** el centro del pot tiene una zona muerta que deja exactamente los 80 BPM
  originales.
- **Pots por panel:** al entrar a un panel ningún pot cambia nada hasta que lo mueves (~5 %).
  Desde ahí la posición física es el valor, y cambiar de panel nunca pisa lo del otro.
- **LEDs de la placa:** 0 melodía · 1 bajo · 2 arpegio (destellan con cada nota) · 3 pulso de
  negra (fuerte en el 1) · 4 panel (general blanco, melodía cian, bajo violeta, arpegio naranjo)
  · 5 verde tocando / rojo tenue detenido / **magenta = sobrecarga de CPU** (ver abajo).

## De dónde salen las notas

El `.mid` exportado (`GC Lab Chile/la partida-victor jara.mid`) trae **una sola pista**: la del
arpegio. Las tres pistas se sacaron del proyecto de Ableton `la partida victor jara.als` con
`extraer_cancion.py`, que respeta lo que suena en el arreglo: posición de cada clip, su ventana,
el loop y el offset interno (el clip de la melodía arranca en la negra 28 de su loop), y corta
las notas que pasan del final del clip. El arpegio extraído coincide nota por nota con el `.mid`.

```bash
python extraer_cancion.py "ruta/a/la partida victor jara.als"
```

Sirve para cualquier otro proyecto con tres pistas MIDI en el orden melodía · bajo · arpegio.
La transposición de cada pista se ajusta con `TRANSP_MELODIA` / `TRANSP_BAJO` / `TRANSP_ARPEGIO`
en el `.ino` (0 = las notas del archivo; la melodía queda en Mi5–Sol6, tal como está en el arreglo).

## Tiempo exacto

480 ticks por negra, 80 BPM, 4/4, 68 negras = **51.00 s**. La posición se cuenta con un
acumulador de 64 bits (32.32) que avanza **una vez por muestra**, así que cada nota cae en la
muestra que le corresponde. Error acumulado del reloj en toda la canción: **0.12 µs**.

## Por qué no hace clicks

- Ninguna fase se reinicia y ningún ataque parte de cero: sube desde donde esté la envolvente.
- El arpegio tiene **una voz por altura**: re-tocar una nota re-dispara esa voz desde donde
  está (amplitud y filtro suben linealmente desde su nivel actual). Nunca se roba una voz, y se
  apaga en dos etapas: a −60 dB pasa a un cierre de 1.5 ms y se libera a −82 dB.
- **Por qué se cambiaron las campanitas:** con decay largo sonaban muchas a la vez y sus parciales
  inarmónicos (2.76 y 5.40) se sumaban en una bruma disonante que se oía como ruido. El pluck
  tiene armónicos afinados y el filtro se cierra con la cola: con decay largo se suma sin ensuciar
  (peor caso medido, todo al máximo: 11 voces, el limitador baja apenas 0.7 dB).
- Pots suavizados por muestra (12 ms); SVF de Zavalishin, estable con el corte en movimiento.
- Resonancia atada al corte: sobre 3 kHz el Q máximo se achica.
- Stop cierra todo con un tau de 40 ms. Ataque mínimo del arpegio: 3 ms.
- Bloqueador de continua, limitador con ganancia suavizada y saturación suave al final.
- Audio en su propia tarea en el **core 1**; botones, pots y LEDs en el **core 0** a 1 kHz. Sin Serial.

## Ruido con el decay del arpegio alto: casi seguro, tiempo de CPU

El ruido siguió después de cambiar las campanitas por el pluck, y la simulación sale limpia en
ese mismo caso: la señal está bien, así que lo más probable es el tiempo. Con decay alto suenan
hasta ~11 notas del arpegio a la vez, y cada una recalculaba su filtro con `tanf`/`powf`, que en
el S3 cuestan cientos de ciclos. Si el render de 128 muestras no cabe en sus 2.9 ms, el DMA se
vacía y el driver manda ceros, y eso se oye como ruido. (En el PC esas funciones son baratas, por
eso la simulación no lo reproduce.) Arreglo: `tanRapido` (Padé), `exp2Rapido`/`log2Rapido` (polinomios, error
< 0.04 %), compensación de Q fija en el arpegio y coeficientes cada 32 muestras. Estimado en el
S3: de ~4300 a ~1700 ciclos por muestra de los ~5400 disponibles.

**Indicador:** si un buffer tarda más del 80 % de su tiempo, el LED 5 destella **magenta**. Si
vuelve a haber ruido y el LED destella, el problema sigue siendo de tiempo; si no destella, es
otra cosa.

**Verificado en simulación** (el `.ino` compilado en el PC con mocks): canción completa, stop y
play a mitad, barrido de todos los pots mientras suena, velocidad ×0.5 y ×2, ataque y decay en
los extremos, cada pista aislada y todos los volúmenes y el decay al máximo. **0 clicks**
(detector de transitorios en alta frecuencia, calibrado con un corte en seco y con un escalón de
−6 dB), **0 NaN, 0 muestras saturadas**, pico entre −14 y −1.4 dBFS según el volumen.

## Requisitos

ESP32 Arduino core ≥ 3.x · **FastLED** · placa *ESP32S3 Dev Module*, USB CDC On Boot *Enabled*,
Flash Mode **DIO**.
