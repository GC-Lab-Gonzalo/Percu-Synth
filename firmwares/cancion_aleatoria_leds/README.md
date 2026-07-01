# cancion_aleatoria_leds — Máquina de canciones aleatorias (autónoma) + melodía + 6 LEDs

Hermano **autónomo** de [`pads_imu_leds`](../pads_imu_leds/): reutiliza su motor de
voces estéreo (pads profundos con detune/paneo + filtro biquad resonante) y su
visualizador de 6 LEDs, **pero quita todo el control manual salvo dos cosas**:

- **BTN1 → PLAY / STOP**
- **POT1 → VOLUMEN de los PADS**
- **POT2 → VOLUMEN de la MELODÍA** (para mezclar pad ↔ melodía)

Todo lo demás es **aleatorio pero coherente**. Al dar **PLAY** se sortea una **canción
completa**; al dar **STOP** y volver a **PLAY** nace una **canción totalmente nueva**.

> El "arpegio pre-configurado" de `pads_imu` desaparece: en su lugar hay una
> **melodía monofónica generativa** de verdad (nota a nota, sin patrón fijo).

## Qué se sortea en cada PLAY

| Ámbito | Qué se aleatoriza |
|---|---|
| **Arquetipo** | Fija el **carácter global** de la canción → cada una suena distinta (ver abajo) |
| **Tonalidad** | 12 tónicas |
| **Modo/escala** | Jónico · Dórico · Frigio · Lidio · Mixolidio · Eólico · Menor armónica |
| **Armonía MODAL** | Progresión con el **vocabulario de acordes propio de cada modo** + su **cadencia característica**, 4–8 acordes (ver abajo) |
| **Clock** | **BPM 46–128** (según arquetipo) + cuadrícula de semicorcheas |
| **Pad** | **Voicing** (tríada / tríada+octava / séptima / abierto) · **ritmo** (sostenido o **gate rítmico** sincronizado al clock) · onda · sub-osc · brillo · detune · ancho estéreo |
| **Filtro** | Piso de cutoff · resonancia · **LFO autónomo** (respiración, sin IMU) |
| **Envolvente pad** | Ataque (swells lentos ↔ ataques punchy) · release |

### Armonía modal (por qué suena coherente)

Un modo **solo suena a ese modo** con su propio vocabulario de acordes: una progresión
funcional genérica (forzar IV/V y cadencia V→I) rompe la modalidad y mete acordes que
chocan. Por eso cada modo tiene su **pool de acordes** (excluye disminuidos/aumentados y
los que rompen el modo) y su **cadencia característica** que resuelve a la tónica al
loopear:

| Modo | Acordes usados | Cadencia |
|---|---|---|
| Jónico (mayor) | I · IV · V · vi · ii | V→I |
| Dórico | i · **IV** · ♭VII · v · ♭III | ♭VII→i |
| Frigio | i · **♭II** · ♭III · ♭VI · ♭vii | **♭II→i** (cadencia frigia) |
| Lidio | I · **II** · V · vi (sin IV) | II→I |
| Mixolidio | I · **♭VII** · IV · v · vi | ♭VII→I |
| Eólico (menor) | i · ♭VI · ♭VII · iv · ♭III | ♭VII→i |
| Menor armónica | i · iv · **V** · ♭VI | V→i (con sensible) |

Las tríadas se construyen desde la escala del modo, así que su **cualidad sale correcta
sola** (♭II mayor en frigio, II mayor en lidio, ♭VII mayor en mixolidio, V mayor en menor
armónica). La melodía usa las 7 notas del modo (los grados característicos = el color) y
las notas largas/acentuadas se enganchan al acorde en curso → nunca choca.

### Arquetipos (uno por canción)

| # | Arquetipo | Carácter |
|---|---|---|
| 0 | **AMBIENT** | Lento, pads sostenidos, melodía larga y espaciada, oscuro |
| 1 | **CINEMATIC** | Swells, acordes lush (7ª/octava), barrido de filtro amplio |
| 2 | **PULSE** | Medio-rápido, **pad rítmico** (gate en blanca/negra) |
| 3 | **PLUCK** | Melodía corta/plucky, pad suave, brillante |
| 4 | **DRIVE** | Rápido, **stabs rítmicos**, filtro resonante |

## La melodía (reemplaza al arpegio)

Es una **línea monofónica** generada en tiempo real. **No hay patrón fijo.** La melodía
se organiza en **frases** (3–9 notas): al abrir cada frase se sortea su **carácter**
(ritmo, timbre, registro, envolvente, brillo), y dentro de la frase ese carácter se
mantiene → variedad **sin caos**. Por frase se sortea:

- **Rango VOCAL humano** → la melodía vive en ~C3–D#5; si una nota se saldría, se **pliega a
  la octava**, y por encima de ~G#4 las notas agudas suenan **más suaves y apagadas** (nunca
  molestas). Nunca sube de octava por frase (a veces baja).
- **Ritmo (notas largas)** → LONG · FLOW · RHYTHM · SPARSE · MOVED, con **mínimo = negra**
  (se quitaron las corcheas/semicorcheas sueltas que sonaban muy cortas) → notas largas y
  cantables, con blancas y hasta redondas en los estilos lentos.
- **Timbre VOCAL FIJO por canción** → la onda de la melodía se sortea **una sola vez al dar
  PLAY** y **no cambia durante la canción**: solo ondas suaves (**triangular / seno** dulces +
  **sierra bien filtrada**, **nunca cuadrada**). El **brillo** sí varía por frase.
- **Afinación** → la melodía va **afinada** (sin el detune del pad; una voz sola no se desafina).
- **Vibrato SIEMPRE presente** → rate **aleatorio y DINÁMICO**: empieza lento (~3.5–5 Hz) y
  **acelera** hacia el final de la nota (~5.5–8 Hz), con **onset retardado** (entra tras el
  ataque, como una voz real). Profundidad ~10–31 cents.
- **Ligados (portamento)** → algunas notas se **ligan** a la anterior con **glide** de
  30–90 ms sin re-atacar.
- **Envolvente** → SOSTENIDA o PLUCK/legato; **sin staccato**, ambas terminan con **caída
  suave** → nunca de golpe.
- **Cuerpo** → algunas frases duplican la melodía una **octava abajo**.

**Coherencia armónica** (por nota): las notas **medias/largas caen SIEMPRE en una nota del
acorde**; las **cortas** pueden ser **de paso** (paso de un grado, vecinas de un acorde) y
**resuelven** al acorde en la siguiente → variedad melódica sin notas que choquen. Si una
nota sostenida queda colgada cuando **cambia el acorde**, **desliza** a la nota del acorde
nuevo (suspensión→resolución). **Dinámica**: velocidad variada + acento en notas largas.

## Qué muestran los 6 LEDs

1. **Color** = tonalidad de la canción; se desplaza con el **acorde** en curso y con el
   **LFO del filtro** (color sweep ↔ filter sweep).
2. **Barra (VU)** = energía del **pad**.
3. **Punto que corre** = avanza una posición por cada **nota** de la melodía (su pulso).
4. Flash suave al **cambiar de acorde** · flash blanco al **arrancar** una canción nueva ·
   sin PLAY → **respiración lenta** (modo espera).

El **LED RGB del módulo** (GPIO48) refleja el promedio de la tira.

## Diferencias con `pads_imu_leds`

| | pads_imu_leds | cancion_aleatoria_leds |
|---|---|---|
| Control | 5 botones + 4 pots + 3 paneles + IMU | **1 botón (Play/Stop) + 2 pots (vol pad / vol melodía)** |
| IMU / MPU6050 | sí (mueve el filtro) | **no** (filtro por LFO interno) |
| Capa melódica | arpegio pre-configurado (6 tipos) | **melodía monofónica aleatoria** |
| Armonía | bancos de acordes fijos | **generada** (tonalidad + modo + progresión) |
| Uso | instrumento tocable | **instalación autónoma**: aprietas y suena |

## Hardware

- ESP32-S3 + DAC **PCM5102** vía I2S (`LCK 39 · DIN 40 · BCK 41`)
- **6 LEDs WS2812** internos (`DATA 46`) + **LED RGB del módulo** ESP32-S3 (`DATA 48`)
- **BTN1** (`44`, pull-up interno) = Play/Stop · **POT1** (`ADC1`) = volumen pads · **POT2** (`ADC2`) = volumen melodía
- **Estéreo real**: filtro del pad con **LFO desfasado 90° en L/R** (barrido distinto por canal) + detune/paneo.
- **No requiere MPU6050 / IMU.**

## Diagnóstico de cuelgues (monitor serie)

El sketch no imprime nada durante la reproducción (todo el CPU al audio), **pero al arrancar
imprime el motivo del último reset** por USB-CDC. Abre el **Monitor Serie a 115200** y tras un
cuelgue, al reiniciar, verás algo como:

```
[BOOT] cancion_aleatoria_leds — motivo del ultimo reset: 11 = BROWNOUT (caida de tension) <-- ALIMENTACION, no es el codigo
```

- **`PANIC` / `INT_WDT` / `TASK_WDT`** → es **código**: justo antes de esta línea, el panic-handler
  de la ESP-IDF imprime el *backtrace*; decodifícalo con **Tools → ESP Exception Decoder**.
- **`BROWNOUT`** → es **alimentación** (LEDs + audio hunden el regulador): mejor fuente/USB,
  condensador de reserva, o baja `LED_BRIGHT`. No se arregla tocando el código.

## Compilar y flashear (Arduino IDE)

- Board: **ESP32S3 Dev Module**
- USB CDC On Boot: **Enabled**
- Flash Mode: **DIO** (¡OPI rompe el I2S!)
- PSRAM: **OPI PSRAM**
- Librerías: ESP32 Arduino core ≥ 3.x + **FastLED** (gestor de librerías)

## Ajustes rápidos

| Síntoma | Ajuste |
|---|---|
| Colores invertidos | `COLOR_ORDER` GRB → RGB |
| Brillo de la tira de 6 | `LED_BRIGHT` (0–255, def 150) |
| Brillo del LED frontal (módulo) | `ONBOARD_BRIGHT` (0–255, def 45) |
| Melodía muy baja / pad muy alto | `melLevel` (↑) y `padLevel` (↓) en `startSong()` |
| Melodía muy densa / muy rala | tablas `STYLE_DUR` y `STYLE_REST` |
| Notas demasiado largas (se eternizan) | `MEL_NOTE_MAX_SEC` (cap absoluto de duración de nota) |
| Repite mucho la misma nota | umbral del anti-repetición (`melRepeat >= 2`) en `melodyStep()` |
| Melodía se corta en seco | `melRel` en `startSong()` (release; ↑ = se apaga más de a poco) |
| Chasquidos con filtro muy dinámico | ya resuelto: los coeficientes del biquad se **interpolan muestra a muestra** (`filterStep`/`filterDelta`). Si aún se oyen, bajar `filtLfoDepth`/`filtLfoRate` o `qBase` por arquetipo |
| Chasquidos en cambios de acorde | ya resuelto: `capPad()` **funde** las voces sobrantes (~6 ms) en vez de cortarlas en seco |
| Estabilidad del filtro | guardas de `applyFilter()` (anti-blowup: reset si NaN/Inf + límite de magnitud) |
| Más/menos variedad tímbrica | rango `lpMin`/`lpMax` por arquetipo · largo de frase (`phraseLeft` en `newPhrase()`) |
| Más/menos vibrato | `melVibDepth` y rango `melVibR0`/`melVibR1` en `melodyStep()` |
| Rango vocal (agudos) | `VOCAL_LO`/`VOCAL_HI`/`SOFT_FROM` |
| Más/menos ligados (glide) | `legatoChance` en `newPhrase()` |
| Melodía repite mucho / poca variedad | baja el `0.22f` de `forceChord` en `melodyStep()` |
| Ondas de la melodía | `MEL_WAVES` (nunca metas la cuadrada = 2); se fija por canción en `startSong()` |
| Ancho del estéreo del pad | desfase del LFO L/R (`+0.25f` en el loop) y `panWidth` por arquetipo |
| Balance pad/melodía | en vivo con **POT1** (pad) y **POT2** (melodía) |
| Qué arquetipos aparecen | `songArche = rndI(5)` en `startSong()` (fija uno para probar) |
| Ritmo del pad más/menos marcado | `padGateDepth` y `padGateSamples` en `startSong()` |
| Canciones más lentas/rápidas | rangos de `songBPM` por arquetipo en `startSong()` |
