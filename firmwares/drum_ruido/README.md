# drum_ruido

Drum machine 100 % sintetizada (sin samples). Los **timbres** son ruidosos a propósito — ruido, chapas metálicas, clanks de fierro, ráfagas de vapor — pero la **cadena final es limpia**: nada de bit-crush, nada de diezmado de sample rate. La suciedad se hace con síntesis y con saturación por bandas, no rompiendo el audio.

Las 7 pistas: **bombo · caja · hats · clank** (fierro corto) **· metal** (yunque) **· ráfaga** (vapor/aire) **· bajo** (sub limpio).

Los **patrones son aleatorios**: no hay ritmos de fábrica, cada BTN1 sortea uno nuevo de 32 pasos (2 compases de semicorcheas) con el "feel" del timbre activo.

## Controles

Cada botón hace **una sola cosa** y actúa en el **flanco de presión**. Cada pot significa **siempre lo mismo**: no hay paneles, ni combos, ni pots congelados.

| Control | Acción |
|---|---|
| **BTN1** (44) | **PATRÓN**: sortea un patrón nuevo (el timbre y el tempo no se tocan). |
| **BTN2** (42) | **TIMBRE**: cicla los 5 kits. Cambia la síntesis al instante; el patrón se mantiene. |
| **BTN3** (0) | **TAP TEMPO**: marca el pulso (negras). Con 2 toques fija el BPM (60–200). El primer toque tras 2 s de silencio reubica el "1". |
| **BTN4** (45) | **MEDIO TIEMPO** (mantener): cada paso del patrón dura el doble, así que el ritmo cae a la mitad de velocidad. Al soltar retoma **sin desfase**. |
| **BTN5** (47) | **FILL** (mantener): redoble que acelera de fusas a semifusas. Bombo y bajo siguen sonando debajo. |
| **POT1** (ADC1) | **VOLUMEN** master |
| **POT2** (ADC2) | **FILTRO**: corte del LPF resonante global (200 Hz → abierto del todo). Al cerrarlo sube la resonancia (hasta 3.2) y **cada golpe abre el corte** → el filtro respira con el ritmo. |
| **POT3** (ADC8) | **SUCIEDAD**: saturación en **dos bandas** (graves/medios hasta ×4.35, agudos apenas) + algo más de ruido en cada golpe. |
| **POT4** (ADC10) | **BEAT REPEAT**: OFF · ×2 (loop de 8 pasos) · ×4 (4) · ×8 (2) · ×16 (1 paso) |

La máquina **siempre suena**: no hay play/stop. Para silenciarla, el POT1 a cero.

El **medio tiempo** no toca el reloj maestro — sigue corriendo por debajo mientras el patrón avanza a la mitad —, así que al soltar el botón se retoma exactamente donde tocaba. Verificado: en 3 s se disparan 29 pasos del patrón en normal y 15 en medio tiempo, con el maestro avanzando 28 en ambos casos, y al soltar `curPlayStep == masterStep`.

El **beat repeat** loopea sobre un reloj maestro que nunca se detiene, así que al volver a OFF la máquina retoma en tiempo, nunca desfasada. Las zonas del pot tienen **histéresis** (el ruido del ADC no puede saltar de división sola) y el ancla **busca hacia atrás un bloque que tenga golpes**: si engancha un tramo vacío del patrón, el break se convertiría en silencio.

## Los 5 timbres (BTN2)

| Kit | Carácter |
|---|---|
| **CYBER** (cian) | Electrónico limpio y punzante. |
| **DUBSTEP** (violeta) | **Half-time**: caja en el 3, bombo grande, sub gordo, patrón escaso. |
| **GLITCH** (verde) | IDM roto: todo cortísimo, mucho blip. |
| **INDUSTRIAL** (naranja) | Metálico y macizo, ametralladora. |
| **CAOS** (rojo) | El más ruidoso — pero sigue siendo audio limpio. |

Cada kit redefine la síntesis de las 7 pistas **y** las densidades con las que se sortea el patrón.

## Las reglas de calidad (no romperlas al editar)

Esto es lo que separa esta versión de la primera, que sonaba mal. Cada punto viene de una medición, no de una corazonada:

- **Dos envolventes por golpe, una para el TONO y otra para el RUIDO.** Es lo que separa un bombo de un "pfff": el ruido de pegada muere en 6 ms mientras el cuerpo grave sigue 200 ms. Con una sola envolvente compartida es imposible.
- **El filtro por voz actúa sólo sobre la capa de ruido.** El tono nunca se filtra, así el fundamental del bombo no se pierde.
- **Los decays son la tau de una exponencial**, o sea que el golpe se oye durante ~3·tau. El bombo original tenía tau 0.42 → sonaba **casi 2 segundos** y dejaba de ser un golpe para volverse un zumbido grave que tapaba todo. Ahora el bombo va de 0.13 a 0.30 (0.6–0.9 s reales) y el hat abierto no pasa de 0.17.
- **PolyBLEP en todo lo que tenga esquinas.** Una cuadrada cruda a 3 kHz devuelve armónicos por encima de Nyquist que se pliegan hacia abajo como tonos inarmónicos: ése es el "chillido que no es de ninguna nota".
- **El bombo termina en ~50 Hz, no en 36.** Un parlante chico no reproduce 36 Hz. Además lleva **saturación propia** (1.25–1.7), que le genera armónicos y lo hace audible aunque no haya graves reales.
- **Sidechain**: cada bombo agacha 3 dB el resto durante ~120 ms. Es lo que hace que el bombo *se sienta* en vez de competir de igual a igual con seis pistas.
- **`TRACK_GAIN[]` fija el balance.** El bajo va contenido (0.62) porque vive en la misma banda que el bombo: subirlo emborrona los graves en vez de sumar peso.
- **Curva del filtro `pFilt^0.55`.** Con una exponencial pura, el punto medio del pot caía en 1.4 kHz y los hats (7–12 kHz) **desaparecían** a mitad de recorrido. Resonancia limitada a 1.6 (nada de silbidos) y tope del pot = abierto del todo.
- **Coeficientes del filtro global interpolados muestra a muestra.** Recalcularlos una vez por buffer hace que el corte salte cada 128 muestras y eso se oye como chasquidos al mover el POT2.
- **Bloqueador de DC a la salida**: los barridos de pitch dejan offset.
- **Los metales se hacen con parciales INARMÓNICOS** (1.41, 2.37, 3.14…), no con múltiplos enteros. Por eso suenan a chapa y no a nota — es la diferencia entre un fierro y un tono con armónicos.
- **Nada de barridos rápidos de pitch en el rango medio.** Un barrido corto de ~1 kHz hacia arriba o hacia abajo se oye literalmente como el "pío" de un pájaro. El clank y el metal sólo caen un 5–8 % en 10 ms (eso es el impacto), y la ráfaga usa **Q bajo** con un barrido corto: un pasa-banda de ruido con Q alto barriendo hacia arriba es un silbido de ave de manual.
- **Saturación en dos bandas** (split a 2.2 kHz). Los graves y medios aguantan hasta ×4.35 de drive y engordan; saturar los agudos con la misma fuerza es exactamente lo que produce el chirrido de aliasing. Es un drive de **carácter, no de destrucción**: una versión con el tope en ×7.5 se probó y era demasiado. Verificado midiendo un bombo aislado con el POT3 de 0 a 1: la energía bajo 100 Hz sube 8–9 dB (el drive trabaja) mientras la banda sobre 12 kHz sube 1–4 dB y se queda 35 dB abajo.
- **Dinámica del filtro**: cada golpe con pegada abre el corte, con más profundidad cuanto más cerrado esté el pot (arriba ya está abierto). No es un LFO que se mueve solo — responde a lo que suena. La resonancia se compensa con `Q^0.35` para que el pico no clipee.

Medido en simulación con el filtro abierto: factor de cresta del patrón completo **2.8–3.7** (pegada real, no una pared aplastada), pico 0.76–0.83, cero NaN, cero muestras al tope incluso con el drive arriba y el filtro barriendo.

## Hardware

- ESP32-S3 + PCM5102 (I2S, estéreo 44.1 kHz / 16-bit).
- 6 LEDs WS2812 SMD internos de la placa (pin 46) — **sólo indicadores**: color = timbre, LED0-4 = familia de golpe, LED5 = pulso, N rojos = división del beat repeat, LED5 amarillo = medio tiempo, blanco = fill.
- **No usa IMU ni Serial**: todo el CPU va al audio.

## Ajustes finos (en el `.ino`)

- Toda la síntesis vive en la **tabla `KITS[5]`**: cada fila es un timbre completo (bombo, caja, hat, perc, zap, ruido, bajo) más sus densidades de patrón. Es la forma rápida de inventar timbres nuevos.
- `TRACK_GAIN[]` = balance entre pistas.
- `posWeight()` decide **dónde** puede caer cada pista dentro del compás: es el "feel" del sorteo.
- `BASS_ROOT_HZ` (41.20 Hz = Mi1) y `BASS_NOTES[]` = la tonalidad del bajo.

## Arduino IDE

Board **ESP32S3 Dev Module** · USB CDC On Boot **Enabled** · **Flash Mode DIO** (¡OPI rompe el I2S!) · PSRAM **OPI PSRAM**.
Librería extra: **FastLED** (gestor de librerías de Arduino).

Compilado y verificado: 390.842 bytes de programa (29 %), 26.060 bytes de RAM (7 %).
