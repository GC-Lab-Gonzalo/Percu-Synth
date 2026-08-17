# paisajes_relax_leds

Máquina de **paisajes sonoros relajantes**: 10 sonidos sintetizados en tiempo real (sin samples) que se pueden **encender y combinar libremente** — viento, mar, campanitas, gotas de agua, grillos, fogata, lluvia, cuenco tibetano, arroyo y pájaros. Todo suena **directo y natural, sin efectos** (sin delay/reverb) para simular ambientes naturales. Cada capa activa pinta además su **propio efecto de luz** sobre **dos anillos de 30 LEDs WS2812** (una tira de 60 separada en dos círculos: uno alumbra hacia afuera, el otro hacia adentro), mezclándose de forma aditiva igual que el sonido.

Al encender arranca **en silencio**: el usuario decide qué sonidos activar. Los **6 LEDs SMD internos de la placa** funcionan como **indicadores de estado** (qué capas están sonando). No usa IMU ni Serial: todo el CPU para el audio y las luces.

## Controles

Cada botón **alterna (toggle)** una capa: pulsación **corta** = capa principal, pulsación **larga** (>0.6 s) = capa alternativa. Todas combinables. Al alternar, los anillos hacen un flash con el color de la capa (brillante = encendida, tenue = apagada).

| Botón | Corta | Larga |
|---|---|---|
| **BTN1** (44) | 🌬️ **VIENTO** | 🔥 **FUEGO** (fogata crepitando) |
| **BTN2** (42) | 🌊 **MAR** (olas) | 🌧️ **LLUVIA** |
| **BTN3** (0)  | 🔔 **CAMPANITAS** (pentatónica) | 🥣 **CUENCO** tibetano (dron armónico) |
| **BTN4** (45) | 💧 **GOTAS de agua** | 🏞️ **ARROYO** (agua corriendo) |
| **BTN5** (47) | 🦗 **GRILLOS** | 🐦 **PÁJAROS** (trinos) |

| Pot | Acción (global, sobre todas las capas activas) |
|---|---|
| **POT1** (ADC1)  | Volumen maestro |
| **POT2** (ADC2)  | Densidad / actividad (ritmo de campanitas, gotas, lluvia, crepitar, ráfagas…) |
| **POT3** (ADC8)  | Color tonal: oscuro ↔ brillante (filtro paso-bajos global) |
| **POT4** (ADC10) | Balance **fondo ↔ eventos**: sube o baja campanitas, gotas, grillos y pájaros sobre los fondos continuos (viento, mar, fuego, lluvia, cuenco, arroyo) |

## LEDs internos de la placa (indicadores de estado)

- **LED 1..5** = BTN1..BTN5: apagado = ninguna capa de ese botón activa · color de la capa **principal** o de la **alternativa** según cuál esté encendida · si están **ambas**, alterna entre los dos colores cada 0.4 s.
- **LED 6** = respiración tenue cian cuando hay al menos una capa sonando.

Colores: viento azul claro · fuego naranja · mar turquesa · lluvia azul · campanitas dorado · cuenco violeta · gotas azul-cian · arroyo verde-agua · grillos verde · pájaros celeste.

## Los 10 sonidos (síntesis)

- **Viento** — ruido por paso-banda errante + silbido resonante; las **ráfagas** son un paseo aleatorio lento con **momentos de calma** (brisa casi nula).
- **Mar** — dos olas lentas superpuestas (período 8–16 s) que abren un filtro sobre ruido; la cresta agrega **espuma** (ruido agudo).
- **Campanitas** — voces de 3 parciales inarmónicos (tipo campana) en **Do mayor pentatónica** (2 octavas), con racimos aleatorios. Detalle: si el **viento** está activo, las ráfagas hacen tintinear más las campanitas (como un móvil de viento real).
- **Gotas de agua** — "plink" breve y fugaz (~80 ms) con barrido de pitch ascendente, directo y sin efectos.
- **Grillos** — 2 grillos (portadoras ~4 kHz) con sílabas moduladas y turnos de canto/silencio. Muy sutiles.
- **Fuego** — fogata **suave y lejana**: rumor grave contenido + crepitar aleatorio, todo pasado por un paso-bajos final.
- **Lluvia** — el POT2 es su **intensidad real**: al mínimo, gotitas sueltas y dispersas ("recién empieza a llover") casi sin colchón; al máximo, aguacero continuo de sibilancia ("shhh") pareja, sin graves que suenen a viento.
- **Cuenco tibetano** — dron **grave** de 4 parciales batientes (fundamental + quinta + octava, nada agudo) con respiración lenta y ataque larguísimo; cada activación sortea la nota raíz (C2/D2/E2/G2).
- **Arroyo** — vertiente calma: dos paso-banda con burbujeo suave sobre ruido, a bajo volumen.
- **Pájaros** — 2 aves con trinos de barridos rápidos (2–4 kHz) y pausas naturales, directo y sin efectos.

## Efectos de luz por capa (mezcla aditiva)

| Capa | Anillo A (hacia afuera) | Anillo B (hacia adentro) |
|---|---|---|
| Viento | banda azul-blanca que fluye al ritmo de las ráfagas | halo tenue |
| Mar | la ola **llena el anillo** en turquesa y "rompe" en espuma blanca | respiración azul profunda en contrafase |
| Campanitas | destello dorado por cada nota | — |
| Gotas | **onda expansiva** desde un punto por cada gota | reflejo tenue simultáneo de la onda |
| Grillos | — | noche azul tenue + chispitas verdes al cantar |
| Fuego | chispas amarillas al crepitar | **fuego vivo** (flicker de brasas, paleta de calor) |
| Lluvia | gotitas azules parpadeando | gotitas azules parpadeando |
| Cuenco | respiración violeta-ámbar lenta | respiración violeta-ámbar lenta |
| Arroyo | corriente verde-agua (ruido perlin) | reflejo tenue |
| Pájaros | destellos celestes saltarines por trino | — |

## Hardware

- ESP32-S3 + PCM5102 (I2S, audio estéreo 44.1 kHz / 16-bit). **Sin IMU.**
- **Tira WS2812 de 60 LEDs en el pin de datos 46**, montada como dos círculos de 30:
  - **Anillo A** = LEDs 0–29 de la tira → alumbra **hacia afuera** (eventos y efectos).
  - **Anillo B** = LEDs 30–59 de la tira → alumbra **hacia adentro** (resplandor ambiente).
  - Los primeros 6 LEDs de la cadena son los SMD internos del PCB → **indicadores de estado** (`START_LED 6`).
  - Si montas los anillos al revés, intercambia `RING_A_START` / `RING_B_START`.
- FastLED corre por el periférico **RMT** (no choca con el I2S). `FastLED.show()` throttleado a ~30 FPS.

## Ajustes finos (en el `.ino`)

- **Brillo de la tira:** `LED_BRIGHT` (140).
- **Largo de los anillos:** `RING_LEN` (30).
- **Umbral de pulsación larga:** `LONG_PRESS_MS` (600 ms).
- **Paisaje inicial:** al final de `setup()` (`layerOn[L_MAR]`, `layerOn[L_CAMPANITAS]`).
- **Balance de cada sonido:** los factores `* 0.x * eXx` al final de cada bloque en `renderAudio()`.

## Arduino IDE

Board **ESP32S3 Dev Module** · USB CDC On Boot **Enabled** · **Flash Mode DIO** (¡OPI rompe el I2S!) · PSRAM **OPI PSRAM**.
Librería extra: **FastLED** (gestor de librerías de Arduino).
