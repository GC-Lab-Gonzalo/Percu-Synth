# grabador_campo — grabadora de campo con efectos en las perillas

El micrófono INMP441 pasa por ganancia, filtro, ecualizador y un efecto, todo en tiempo real, y
se graba a una **microSD**. Lo que oyes en los audífonos es exactamente lo que se graba. Pensado
para salir a terreno (un cerro, un río, un bosque), filmar la placa con el teléfono y que en el
video se vea mover una perilla y cambiar el ambiente.

Cada toma deja **dos archivos que empiezan en la misma muestra**:

| Archivo | Qué es |
|---|---|
| `CAMPO_0001.WAV` | El sonido **procesado**: estéreo, 24 bit / 44.1 kHz |
| `CAMPO_0001_CRUDO.WAV` | El micrófono: mono, 24 bit / 44.1 kHz, sin ganancia, efectos ni limitador; sólo pasa por el techo de agudos de 15.5 kHz (`FILTRAR_CRUDO 0` lo deja intacto). Es la calidad real del micrófono, y te deja reprocesar la toma si el efecto no quedó como querías |

En el editor se ponen uno encima del otro y calzan muestra a muestra. Para sincronizar con el
video: **un aplauso frente al micrófono al empezar**. La numeración sigue la que ya haya en la
tarjeta.

## Controles

| Control | Qué hace |
|---|---|
| BTN1 | **Grabar / parar** |
| BTN2 | **Efecto** siguiente: NATURAL → ECO → CATEDRAL → CHORUS → FLANGER → PHASER. El sonido directo nunca se corta al cambiar |
| BTN3 | **Congelar** on/off: toma los últimos ~3 s del ambiente y los sostiene como una nube continua. El filtro, el EQ y el efecto siguen actuando sobre ella |
| BTN4, BTN5 | Libres |
| POT1 | **Ganancia** +6 … +60 dB |
| POT2 | **Filtro**. Centro = abierto · izquierda pasa-bajos (20 kHz → 120 Hz) · derecha pasa-altos (20 Hz → 5 kHz) |
| POT3 | **Color** (EQ de inclinación). Centro = plano · izquierda cálido · derecha brillante (±8/10 dB) |
| POT4 | **Cantidad** del efecto. ECO: nivel y repeticiones · CATEDRAL: mezcla y tamaño · CHORUS: mezcla · FLANGER: mezcla y realimentación (de suave a metálico) · PHASER: profundidad de las muescas y realimentación |

| LED | Significado |
|---|---|
| 0 | verde tenue = lista · **rojo parpadeando = grabando** · naranjo = se perdió audio (la tarjeta no alcanzó) · amarillo rápido = cerrando, no apagues · magenta = sin tarjeta o error |
| 1 | efecto: blanco NATURAL · celeste ECO · azul CATEDRAL · verde CHORUS · violeta FLANGER · ámbar PHASER |
| 2 | cian = congelado |
| 3–5 | nivel de entrada: verde > −40 dBFS · amarillo > −18 · rojo > −3 (baja el POT1) |

## Conexión de la microSD

| Módulo | ESP32-S3 |
|---|---|
| SCK | GPIO 14 |
| MOSI | GPIO 15 |
| MISO | GPIO 16 |
| CS | GPIO 17 |
| VCC | 5 V (módulo azul con regulador) o 3.3 V (módulo sin regulador) |
| GND | GND |

Tarjeta **FAT32** (hasta 32 GB viene así; una de 64 GB hay que formatearla FAT32). Clase 10 / A1
o mejor. Cada hora de grabación ocupa ~1.4 GB (procesado 24 bit + crudo). Los pines están en
`#define SD_*` al inicio del `.ino`.

El micrófono va en WS 11 · SCK 12 · SD 13 (el mismo cableado que `voz_fx`).

## En terreno

- **Viento**: el INMP441 es diminuto y cualquier brisa lo satura. Una espuma como mínimo; una
  "peluda" (dead cat) de verdad. El pasa-altos fijo de 30 Hz del procesado ayuda, pero no salva
  una toma soplada (y el crudo no lo lleva).
- **Ruido de las perillas**: si el micrófono va soldado en la misma placa que los pots, el roce
  al girarlos se graba. Sácalo con un cable de 20–30 cm.
- **Ruido propio**: el INMP441 tiene 61 dB de relación señal/ruido. En un lugar muy callado su
  soplido se oye. Es un límite del micrófono, no del grabador: el crudo lo muestra tal cual.
- **Energía**: un power bank por USB. Sin tira de LEDs la placa consume poco.
- **Batería o tarjeta que se cae a mitad de toma**: las cabeceras de los WAV se reescriben cada
  5 s, así que el archivo se abre igual y se pierden a lo más los últimos segundos.

## Decisiones

- **El micro corre a 44.1 kHz con la misma fuente de reloj que el DAC** (como en `voz_fx`): no
  derivan y no hay que remuestrear.
- **La tarjeta nunca toca el audio.** El audio (core 1) deja cada bloque en un colchón de **8 s en
  PSRAM**, y una tarea del core 0 lo vacía a la tarjeta en escrituras de ~24 KB. Una microSD a
  veces se toma 100–300 ms en una escritura; con el colchón eso no se oye ni se pierde. Si la
  tarjeta se traba más de 8 s, se pierden bloques enteros (los dos archivos igual, así siguen
  alineados) y el LED 0 se pone naranjo.
- **Al cambiar de efecto sólo entra y sale la parte húmeda** (fundido de 20 ms): el ambiente
  directo nunca se corta, así que en la grabación no hay huecos.
- **El eco tiene tiempo fijo** (420 ms, `ECO_MS`): moverlo con un pot desafina las repeticiones.
  Es ping-pong: las repeticiones alternan izquierda y derecha.
- **Modulaciones con velocidad fija** (`CHORUS_HZ` 0.35, `FLANGER_HZ` 0.12, `PHASER_HZ` 0.20): un
  pot, una función, y el POT4 ya es la cantidad. Las tres son estéreo:
  - **CHORUS**: tres copias con retardo que se mece (13/17/23 ms ± 2.2 ms), a velocidades que no
    se alinean nunca (×1, ×1.31, ×0.77); una a cada lado y la tercera al centro.
  - **FLANGER**: peine con realimentación de 0.8 a 7 ms, izquierda y derecha en cuadratura (el
    barrido gira en el estéreo). Saturación suave dentro del lazo y el nivel compensado por la
    realimentación.
  - **PHASER**: 6 pasa-todo de 1er orden por canal barridos entre 200 Hz y 3 kHz. El coeficiente
    se calcula al borde del bloque y se interpola por muestra: un `tanf` por muestra y canal sería
    caro en el S3. Mezcla 0.5·(directo + pasa-todo), así con POT4 al máximo las muescas son
    completas y los picos quedan en 0 dB.
  - El shimmer (reverb con octava arriba) se probó y se sacó.
- **Congelar** son 6 granos Hann de 350 ms desfasados, que leen posiciones al azar de los últimos
  ~3 s, con un fundido de igual potencia de 250 ms. Un grano nunca cruza el punto donde el
  anillo junta lo más nuevo con lo más viejo (sería un salto).
- **Techo de agudos en 15.5 kHz** (Butterworth de 8º orden, 4 biquads: plano hasta 14 kHz, −7 dB
  en 16, −37 dB en 18). La primera versión no tenía techo, para no cortarle agudos a los pájaros,
  y en las tomas apareció un soplido fino: medido, el piso de ruido del INMP441 sube 4–7 dB entre
  16 y 20 kHz, justo donde el micro ya no capta nada útil. `voz_fx` no lo tenía porque su
  pasa-bajos de 13 kHz se lo comía. Va ANTES del limitador: un Butterworth de 8º orden rebota un
  poco en los picos, y puesto después llegaba a tope sin que nadie lo atajara (medido en
  simulación). También va sobre el crudo (`FILTRAR_CRUDO`). Limitador con lookahead a −1 dBFS.
- Ajustes en constantes al inicio del `.ino`: `BITS_PROCESADO` (24, o 16 si la tarjeta no da
  abasto), `GRABAR_CRUDO`, `GANANCIA_MIN_DB`/`GANANCIA_MAX_DB`, `ECO_MS`, `CHORUS_HZ`, `FLANGER_HZ`, `PHASER_HZ`.

## Verificado en simulación

Compilado en el PC con un ambiente sintético (ruido rosa a −62 dBFS, pájaros, un aplauso, 5 s de
viento) y un guion de 85 s que recorre los efectos, congela mientras barre el filtro y
después cambia efecto, congelar y los cuatro pots a la vez cada 0.7 s:

- 0 NaN, 0 muestras en tope con ganancias normales; pico 0.93 con el limitador a 0.89.
- Los cinco efectos al máximo con ruido fuerte y después silencio: todos se apagan solos.
- Nivel de cada efecto con POT4 al máximo, contra NATURAL: todos dentro de ±3 dB (el eco un poco
  más fuerte, porque suma repeticiones), así cambiar de efecto no pega saltos de volumen.
- La tarjeta trabándose 400 ms cada 5 s: 0 bloques perdidos. Trabada 10 s: se pierden 2.0 s (lo
  que no cabe en el colchón de 8 s) y los dos archivos siguen del mismo largo.
- Los WAV se abren con cabecera y tamaño correctos. Con `FILTRAR_CRUDO 0` el **crudo es bit a bit**
  lo que entró por el micrófono; con el techo puesto, coincide con la entrada filtrada por el mismo
  Butterworth calculado aparte.
- Tarjeta que deja de aceptar datos a mitad de toma: se cierra, el LED queda en magenta y el
  audio sigue sonando.
