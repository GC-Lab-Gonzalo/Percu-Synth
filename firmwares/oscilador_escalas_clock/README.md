# oscilador_escalas_clock — Dron de 4 osciladores por escala + reloj MIDI por el DIN-5

Copia de [`oscilador_escalas`](../oscilador_escalas/) con tres cosas más: **MIDI Clock por el
DIN-5** que sigue al tap tempo y a la intermitencia, los **6 LEDs de la placa prendiendo al
ritmo** de esa intermitencia, y el **LED RGB del módulo ESP32-S3 con el color de la nota**
que suena. Todo lo demás — los 4 pots = 4 osciladores cuantizados a la escala, las 10
escalas, el unísono, el filtro por IMU, el delay ping-pong — es idéntico al base, y se
mantiene la regla de controles directos: **un botón, una función, sin paneles ni combos**.

## Controles

| Control | Qué hace |
|---|---|
| **POT1..POT4** | Nota del oscilador 1..4, cuantizada a la escala (4 octavas, 28 notas). Al mínimo ese oscilador queda en silencio |
| **BTN1** | **Escala** siguiente (ciclo de 10) |
| **BTN2** | **Octava** global (ciclo de 4: −1 · 0 · +1 · +2) |
| **BTN3** | **Intermitencia** ON/OFF. **ON manda MIDI Start · OFF manda MIDI Stop** |
| **BTN4** | **Tap tempo** de la intermitencia **y del MIDI Clock** (2 golpes o más). Cada tap reengancha el "1": el corte cae donde tú marcas y el sinte externo recibe **Stop+Start** para caer en el mismo pulso. Sin tocarlo, 120 BPM |
| **BTN5** | Toque: **forma de onda** siguiente · Mantenido > 0.8 s: **tónica +1 semitono** |
| **IMU** | El **eje X** abre y cierra el filtro pasa-bajos resonante |

## Reloj MIDI (DIN-5, GPIO 43)

Por el DIN-5 sale **sólo reloj**, nada de notas: **MIDI Clock a 24 PPQN** más **Start / Stop**.
El pulso del clock es el pulso de la intermitencia, así que el arpegiador o el secuenciador
del sinte externo cae exactamente en los cortes.

| Evento | Qué sale por el DIN-5 |
|---|---|
| Siempre (aun parado) | Clock 0xF8, 24 por pulso, al tempo del tap. Así el sinte ya tiene el tempo cuando llegue el Start |
| BTN3 → intermitencia ON | **Start** (0xFA), alineado con el primer pulso abierto |
| BTN3 → intermitencia OFF | **Stop** (0xFC) |
| Tap (BTN4) con la intermitencia activa | **Stop + Start**: el "1" del sinte se reengancha con el tuyo. El Stop antes del Start no es adorno: hay equipos que ignoran un Start si creen que ya corren |
| Tap con la intermitencia apagada | Sólo cambia el tempo del clock. Nada de Start: el transporte lo manda BTN3 |

Cómo está hecho, porque es lo que hace que no derive:

- El reloj **se cuenta en la tarea de audio con el mismo contador de muestras que el corte**
  (un acumulador entero: 24 ticks por período del corte, cero deriva entre ambos), pero **se
  manda desde la tarea de control a 1 kHz**. Mandarlo desde el render metería ±2,9 ms de
  fluctuación por tick (el tamaño del buffer), que a 120 BPM es un 14 % de un tick y se oye
  como un arpegio que tambalea — la lección viene de `drum_poder`.
- Al reenganchar el "1" la tarea de audio marca en qué tick cae el pulso; la de control
  descarta los ticks anteriores y manda el Start justo antes del tick del "1".
- Sale por **`Serial1`** enrutado al GPIO 43, no por `Serial0`: `Serial0` comparte ese TX
  con el DIN-5 y su RX es el 44, que es el BTN1. Este firmware **no imprime nada** por ningún
  puerto (un `Serial.print` por USB CDC bloquea el núcleo hasta que el host lee).
- Medido en simulación: 48 ticks/s a 120 BPM y 60 ticks/s a 150 BPM, 24 ticks entre pulsos,
  Start dentro del buffer siguiente al botón (< 3 ms).

Si tu sinte se vuelve loco con clock entrante, `ENVIAR_MIDI_CLOCK = false` apaga todo el
bloque.

## LEDs

| LED | Muestra |
|---|---|
| 0–3 | Los 4 osciladores: **color = nota**, brillo = nivel. Apagado = ese oscilador está en silencio |
| 4 | Escala activa (color) + octava (brillo) |
| 5 | Apertura del filtro (IMU) + latido del tempo |
| **Los 6 con la intermitencia activa** | **Prenden y se apagan con el corte**: siguen la misma envolvente que corta el audio, así que se ven exactamente como suenan (rampas de 3 ms incluidas) |
| **LED RGB del módulo (GPIO 48)** | **El color de la nota**: muestra uno de los osciladores que están sonando — el mismo color que su LED 0..3 — y **en cada pulso del tempo pasa al siguiente oscilador activo**. Con un solo oscilador queda fijo; sin ninguno se apaga. Con la intermitencia activa late con ella |

Flash blanco en todos (incluido el del módulo) al cambiar escala, octava, onda o tónica.

## Arquitectura: audio en el core 1, controles en el core 0

Esta copia y el original comparten la corrección del **ruido al cambiar de onda con los 4
osciladores sonando** (desaparecía al bajar el oscilador 4 y volvía al subirlo). No era un
problema de señal, era de **tiempo**: todo corría en `loop()` — el render de 128 muestras,
16 lecturas de ADC por vuelta (cada `analogRead` del S3 cuesta decenas de µs), el I2C del
IMU y `FastLED.show()`. La sierra es la onda más barata (un PolyBLEP por voz); cuadrada y
pulso cuestan el doble (dos PolyBLEP cada una). Con 4 osciladores × 3 voces la vuelta se
pasaba de los 2,9 ms del buffer, el DMA se vaciaba y el driver (en `auto_clear`) sacaba
ceros: eso es el ruido. Silenciar un oscilador ahorraba justo lo que faltaba.

Ahora el **audio corre en su propia tarea fijada al core 1** y **botones, pots, IMU, LEDs y
MIDI en una tarea en el core 0 a 1 kHz**. La tarea de control nunca toca los osciladores ni
el reloj: deja *pedidos* (`reqNote[]`, `reqTapMs`, `reqResync`, escala/octava/tónica/onda)
que el audio aplica en el borde de cada buffer. Son escrituras de 32 bits alineadas, atómicas
en el S3, sin mutex.

También cambió el tap tempo: un intervalo que se aparta más del 35 % del tempo en curso abre
una serie nueva sin tocar el tempo. Antes la pausa entre dos series se promediaba como si
fuera un pulso y arrastraba el tempo durante varios taps (4 taps a 400 ms terminaban en
428 ms) — con un sinte externo colgado del clock eso se nota.

## Hardware

- ESP32-S3 + DAC **PCM5102** vía I2S (`LCK 39 · DIN 40 · BCK 41`)
- **MIDI OUT DIN-5** (`TX 43`, 31250 baud, sólo reloj)
- **MPU6050** por I2C (`SDA 21 · SCL 38`) → filtro (eje X)
- **6 LEDs WS2812** internos (`DATA 46`) + LED RGB del módulo (`DATA 48`)
- 5 botones con pull-up (`44 · 42 · 0 · 45 · 47`) · 4 potenciómetros (`ADC 1 · 2 · 8 · 10`)

## Compilar y flashear (Arduino IDE)

- Board: **ESP32S3 Dev Module** · USB CDC On Boot: **Enabled** · **Flash Mode: DIO** · PSRAM: OPI
- Librerías: **FastLED** (`Wire.h` y el driver I2S vienen en el core ESP32 ≥ 3.x)
