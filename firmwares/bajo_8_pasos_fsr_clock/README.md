# bajo_8_pasos_fsr_clock — Bajo de 8 pasos + FSR al filtro + reloj MIDI

Es **`bajo_8_pasos` con dos cosas encima, y nada más**: un **FSR (sensor de presión) en
EXT1 que abre el filtro** y un **MIDI Clock por el DIN-5**. El motor de sonido, los
controles, los paneles y las balas de luz de la tira son idénticos al original — con el
FSR suelto suena exactamente igual.

## Lo nuevo

### FSR → filtro (EXT1, GPIO 3)

Aprieta y el filtro se abre: hasta **3 octavas (`FSR_OCT`) por encima de donde dejaste el
POT4**. El pot sigue poniendo el punto de reposo y la resonancia, como siempre; el FSR es
una expresión en vivo encima, igual que el IMU en `espacio_modular`. Suelto no hace nada
(hay una zona muerta del 6 % que se come el ruido del ADC), así que **el sonido de reposo
es el del `bajo_8_pasos` original**.

Si lo prefieres al revés — que el FSR **sea** todo el cutoff y el POT4 deje de hacer
nada — pon `FSR_REEMPLAZA_POT` en 1 y vuelve a flashear.

Se muestrea a **1 kHz en la tarea de control (core 0)**, una lectura cruda por pasada: el
promediado lo hace el filtro de un polo (`FSR_SUAVIZADO`), que además es lo que evita que
el ruido del ADC se oiga como un temblor. En el audio entra por un suavizado más, por
muestra, así que un golpe de presión no produce escalones.

**El brillo del LED 4 sigue al FSR**, o sea que se ve si el sensor responde sin abrir el
monitor serie.

#### Cableado

Divisor de tensión con una resistencia de **10 kΩ**. Las dos formas sirven:

```
3.3V ── FSR ──┬── EXT1 (GPIO 3)        apretado = lectura ALTA
             10 kΩ
              │
             GND

3.3V ── 10 kΩ ──┬── EXT1 (GPIO 3)      apretado = lectura BAJA
                FSR
                 │
                GND
```

**La polaridad no importa**: el mapeo es una recta entre las dos lecturas medidas
(`FSR_SUELTO` y `FSR_APRETADO`), así que si te queda al revés basta con que `FSR_SUELTO`
sea el número más grande.

#### Calibrar (una vez, al armarlo)

1. Pon `MOSTRAR_ESTADO` en **1** y flashea. Monitor serie a **115200**.
2. Con el FSR **suelto**, mira `raw` → ese número va en `FSR_SUELTO`.
3. **Apriétalo lo que lo vayas a apretar tocando**, mira `raw` → va en `FSR_APRETADO`.
4. Vuelve `MOSTRAR_ESTADO` a **0** y flashea de nuevo.

El punto 4 no es opcional: un print por USB CDC **bloquea el core que lo hace** hasta que
el PC lea, y eso vacía el DMA del audio — se oye como un clic. En 0 no se compila ni una
línea de `Serial`.

### Reloj MIDI por el DIN-5 (GPIO 43)

**Sólo reloj, nunca notas**: MIDI Clock a **24 PPQN** al tempo del secuenciador (POT2 del
panel C), **Start** al dar Play y **Stop** al parar. Como cada paso es una semicorchea,
van **6 ticks por paso** y 24 por negra.

- El **"1" del Start cae en el paso 1**: al dar Play se descartan los ticks anteriores y
  el Start sale justo antes del tick del "1", así la secuencia del sinte externo no queda
  corrida contra el bajo.
- **Los ticks salen siempre, aun parado**, para que el sinte ya tenga el tempo cuando
  llegue el Start y no arranque buscando el pulso.
- El reloj se **cuenta en la tarea de audio con el mismo contador de muestras que los
  pasos** (acumulador entero: no hay dos relojes, hay uno) pero se **manda desde la tarea
  de control a 1 kHz**. Mandarlo desde el render le metería al clock los ±2,9 ms de
  fluctuación del buffer, que a 120 BPM es un 14 % de un tick y se oye como un arpegio
  que se tambalea.
- `ENVIAR_MIDI_CLOCK` en `false` lo apaga, por si tu sinte se vuelve loco con clock
  entrante.

El MIDI sale por **`Serial1` enrutado al GPIO 43**, no por `Serial0`: `Serial0` comparte
ese TX con el DIN-5 y su RX cae en el GPIO 44, que acá es el BTN1. Por eso los prints de
calibración van sólo por el **USB CDC** (`Serial`).

## Controles

| Control | Qué hace |
|---|---|
| **BTN1** | **Play / Stop**. Play arranca desde el paso 1 y manda **MIDI Start**; Stop manda **MIDI Stop** |
| **BTN2** | **Panel A** — POT1..POT4 = nota de los pasos **1..4** |
| **BTN3** | **Panel B** — POT1..POT4 = nota de los pasos **5..8** |
| **BTN4** | **Panel C** — POT1 **volumen** · POT2 **velocidad** · POT3 **decay** · POT4 **cutoff** |
| **BTN5** | **Octava** siguiente (ciclo de 3: La1 · La2 · La3). Cambia también la nota que está sonando |
| **FSR** | **Abre el filtro** por encima del POT4 (hasta 3 octavas) |

- **Notas:** cada pot recorre 11 notas de la pentatónica menor (La · Do · Re · Mi · Sol,
  dos octavas más la tónica de arriba). Con el pot **al mínimo** (< 3 %) el paso queda
  **en silencio**.
- **Velocidad:** 60–200 BPM, cada paso es una semicorchea. Arranca en 120. **Es también
  el tempo del MIDI Clock.**
- **Decay:** 30 ms – 1.2 s. El filtro se cierra un poco antes que el volumen.
- **Cutoff:** 50 Hz – 6 kHz (reposo). La resonancia va atada al corte.
- Al entrar a un panel, **ningún pot cambia nada hasta que lo muevas** (unos 5 %).

## LEDs

| LED | Muestra |
|---|---|
| 0–3 | Panel A/B: los 4 pasos del panel (color = nota, apagado = silencio, **blanco = paso sonando**) · Panel C: nivel de cada parámetro |
| 4 | Panel activo: A cian · B violeta · C naranjo — **el brillo sigue al FSR** |
| 5 | Octava (brillo) + pulso en cada paso mientras suena (fuerte en los pasos 1 y 5). Rojo tenue = detenido |

## Tira externa de 121 LEDs — 8 efectos que se sortean solos

La tira va **encadenada después de los 6 LEDs SMD de la placa, en el mismo pin (GPIO 46)**:
índices 6..126.

| Efecto | Qué hace |
|---|---|
| **BALAS** | Un proyectil con estela por cada paso, volando al tempo (el de `bajo_8_pasos`) |
| **ONDAS** | Anillos que se abren hacia los dos lados desde un punto al azar |
| **VU** | Barra que crece desde el centro con la envolvente, con punta blanca en el golpe |
| **CHISPAS** | Puñados de píxeles que se encienden en el golpe y se apagan solos |
| **ARCOÍRIS** | El espectro entero corriendo al tempo |
| **PLASMA** | Campo de ruido que respira |
| **BANDAS** | La tira partida en 8 bloques, uno por paso, en orden barajado |
| **SERPIENTE** | Una cabeza blanca que va y vuelve al tempo, con su cola |

**Se sortean solos**: uno nuevo cada **4 compases** y en cada **Play**, y nunca sale el que
estaba sonando. Con el efecto se sortean su paleta, su dirección y un parámetro propio, así
que el mismo efecto **no se ve igual dos veces**. No hay botón para cambiarlo — los cinco ya
tienen su función y acá no se inventan combos.

**Los ocho son reactivos a lo mismo**, así que da igual cuál salga:

- el **golpe** de cada paso (más fuerte en los pasos 1 y 5, que marcan el compás),
- la **nota** que suena, que define el color,
- la **envolvente real del bajo** — la tarea de audio publica su envolvente en `g_env` una
  vez por buffer, así que la tira sigue al sonido de verdad y no a una imitación calculada
  aparte en el core 0,
- la **presión del FSR**, que deforma el efecto en vivo (alarga estelas y colas, ensancha
  anillos y bloques, comprime el arcoíris, calienta y acelera el plasma, llena de polvo las
  chispas),
- y el **tempo**, que fija cuántos LEDs se recorren por paso (`LEDS_POR_PASO`, 15: los
  8 pasos caben justo en la tira).

Un paso en silencio no dispara nada. Todo el dibujo corre en la **tarea de control (core 0)**
a ~60 fps con movimiento sub-píxel; el audio no se entera.

Se ajusta en el `.ino`: `TIRA_LEDS`, `LEDS_POR_PASO`, `PASOS_POR_SORTEO`, `HUE_PASO[]`.

### Alimentación y el limitador de corriente — léelo antes de subir el brillo

**Si la tira cuelga del 5 V de la placa (o del USB), un efecto que prende los 121 LEDs a la
vez pide del orden de 2 A, el riel se hunde y se corta el audio.** Las balas nunca lo
provocaban porque encienden ~50 LEDs con una estela que se apaga; ARCOÍRIS y PLASMA
encienden los 121 siempre, y BANDAS también cuando aprietas el FSR.

Ojo con el diagnóstico, porque se parece al otro: **esto no es el problema de tiempo** que
hay documentado en `drum_poder` y `la_partida`. Los ocho efectos juntos no llegan a 0,5 ms
de los 16 que hay entre frames, y el audio corre en el otro núcleo. Es eléctrico.

Por eso la tira tiene **su propio limitador**, que mide lo que el frame va a consumir y lo
baja si se pasa. Está escrito como un limitador de audio y por el mismo motivo: **baja al
instante** (pasarse aunque sea un frame ya es el bajón) y **sube despacio**, porque un
escalón de corriente hacia arriba es justo lo que hace caer el riel. No toca los 6 LEDs de
estado.

| Constante | Para qué |
|---|---|
| `TIRA_MAX_MA` | **El número a tocar.** 300 mA por defecto, pensado para un puerto USB de 500 mA con la placa consumiendo ~150 |
| `LED_MAX_MA` | Tope duro de FastLED (600 mA), como red de seguridad |

**Si le pones fuente propia a la tira** (5 V, 2 A o más, GND común con la placa), sube
`TIRA_MAX_MA` a 1500 y `LED_MAX_MA` a 2000 y los efectos vuelven a brillo completo. Y aunque
tenga fuente propia, conviene un **condensador de 1000 µF** en la entrada de la tira: lo que
hunde el riel no es la corriente alta, es el **escalón** de corriente.

Consecuencia de tener el limitador puesto: con poco presupuesto, apretar el FSR ya no hace
que la tira brille *más* — **redistribuye** la luz (colas más largas, bloques que desbordan,
el arcoíris que se comprime) manteniendo la energía total. Con fuente propia vuelve a ser
las dos cosas.

## Cadena de audio

`sierra + sierra (+9 cents) + cuadrada −1 oct → saturación suave → SVF pasa-bajos
(cutoff pot × envolvente de filtro × FSR, Q atado al corte) → VCA (ataque 2 ms + decay) →
bloqueador de continua → volumen → limitador suave`

El FSR viaja en **el mismo exponente** que la envolvente de filtro, así que no cuesta ni
un `powf` extra (y `powf` en el S3 se paga caro — ver la lección de `la_partida`).

## Verificado

Compila para ESP32-S3 (30 % de flash, 6 % de RAM) con `MOSTRAR_ESTADO` en 0 y en 1, y
corrido en simulación en el PC contra mocks de `Arduino.h` / `i2s_std.h` / `FastLED.h`:

- **Reloj al tempo**: 24,00 tick/s a 60 BPM, 48,10 a 120,2, 80,01 a 200 → error **< 0,06 %**.
- **Cero deriva**: 60 s tocando a 60 / 120 / 200 BPM → **6,000 ticks por paso** (esperado 6).
- **Transporte**: Start al Play sin ningún clock antes del "1", el primer tick tras el
  Start es el paso 1, un solo Stop al parar, y los ticks siguen saliendo detenido.
- **FSR**: apertura monótona del filtro con la presión (×2,3 de agudos de suelto a fondo),
  **0 muestras saturadas** y 0 NaN en los 11 puntos del barrido y en 40 golpes bruscos de
  0 % a 100 % con el secuenciador tocando; pico máximo −7,9 dBFS.
- **FSR suelto = original**: la aportación al filtro es exactamente 0, y un 4 % de
  presión (zona muerta) tampoco mueve nada.
- **Tira**: los 8 efectos encienden la tira (de 53 a 121 píxeles de media según el efecto)
  y **ninguno apaga los 6 LEDs de estado** de la placa; el sorteo **nunca repite** el efecto
  anterior y en 4000 tiradas salieron los 8; `faseFx` nunca queda en NaN.
- **Reactividad al FSR de los 8**, medida como cambio de brillo total y de "dibujo" (la
  suma de diferencias entre píxeles vecinos, que es lo que se mueve cuando el efecto se
  comprime o se corre de color en vez de encenderse más). Hacen falta las dos medidas: el
  ARCOÍRIS y el PLASMA responden a energía casi constante y, midiendo sólo brillo, parecían
  muertos sin estarlo — y con el limitador de corriente puesto eso pasa a ser lo normal,
  porque la energía total queda acotada.
- **Presupuesto de corriente**: con el FSR a fondo, ningún frame de ninguno de los 8 efectos
  pasa de los 300 mA (picos de 286 a 297 mA, medias de 185 a 282), y al soltar un efecto
  caro la corriente sube como mucho 70 mA entre frame y frame en vez de dar un escalón.

## Hardware

- ESP32-S3 + DAC **PCM5102** vía I2S (`LCK 39 · DIN 40 · BCK 41`)
- **6 LEDs WS2812** internos (`DATA 46`) + **tira WS2812 de 121 LEDs** encadenada después (mismo pin)
- 5 botones con pull-up (`44 · 42 · 0 · 45 · 47`) · 4 potenciómetros (`ADC 1 · 2 · 8 · 10`)
- **FSR + 10 kΩ** en el sensor externo A (`EXT1 → GPIO 3`)
- **MIDI DIN-5** (`TX → 43`, 31250 baud)

## Compilar y flashear (Arduino IDE)

- Board: **ESP32S3 Dev Module**
- USB CDC On Boot: **Enabled**
- **Flash Mode: DIO** (¡OPI rompe el I2S!)
- Librerías: **FastLED** (el driver I2S viene en el core ESP32 ≥ 3.x)
