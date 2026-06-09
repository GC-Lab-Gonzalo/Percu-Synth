# seismic_drone — Drones épicos por vibración de la tierra

Hermano ambiental y **grave** de [`impact_chimes`](../impact_chimes/). Se apoya el
PercuSynth en el **piso** y el **acelerómetro** —configurado en su rango **más sensible
(±2g)**— escucha la **vibración de la tierra**: pasos, golpes lejanos, retumbar, manos
sobre el suelo. Esa vibración no toca "campanas": **alimenta un dron**.

- Hay un **dron de base siempre presente** (tónica + quinta) que **respira con la tierra**:
  cuando el suelo vibra, el dron crece y el filtro se abre; en silencio queda oscuro y tenue.
- Los golpes más marcados, además, **siembran notas-pad** de la escala que entran **lento,
  con swell** (ataque ~550 ms, nunca al choque) y se solapan en una textura espacial y épica.
- Motor: I2S → PCM5102 **44.1 kHz / 16-bit estéreo**. Cada voz = **3 osciladores diente de
  sierra** (muchos armónicos) ligeramente **desafinados** + un **sub seno** una octava abajo
  (peso grave). El desafine reparte los osciladores entre L y R → **ancho estéreo espacial**.
  Todo pasa por un **filtro paso-bajos resonante** (lows profundos + un pico de agudos que
  silba como un coro/formante = lo "épico").
- Base muy grave: **A1 = 55 Hz**. Sin LEDs, sin Serial.

## Controles

### Vibración (acelerómetro, ±2g)
Apoya el equipo en el piso. El temblor del suelo **hace respirar al dron** (volumen + brillo
del filtro). Mientras más vibra la tierra, más abierto y épico. Un golpe marcado **siembra**
una nota-pad de la escala (camina suave por la escala) con cola larga.

### Escala — un botón por escala épica *(naturaleza y universo)*

| Botón | Pin | Escala | Carácter |
|---|---|---|---|
| BTN1 | 44 | **NEBULOSA** — eólica (menor natural) *(por defecto)* | oscura, vasta |
| BTN2 | 42 | **GALAXIA** — lidia (#4) | flotante, celeste, expansiva |
| BTN3 | 0  | **ECLIPSE** — menor armónica | dramática, tensa |
| BTN4 | 45 | **OCÉANO** — dórica | terrenal, heroica |
| BTN5 | 47 | **COSMOS** — frigia | antigua, profunda, ritual |

### Textura — potenciómetros

No hay control de ataque (el pad entra lento solo, ~550 ms de swell); en su lugar controlas la **cola** y la **forma**:

| Pot | Pin | Función |
|---|---|---|
| POT1 | ADC1 | **Desafine / ancho** (unísono fino → super-saw ancho y espacial) |
| POT2 | ADC2 | **Cola del dron** (pad ~1 s → dron casi infinito ~30 s) |
| POT3 | ADC8 | **Brillo / cutoff** (retumbar oscuro → sierra abierta y brillante) |
| POT4 | ADC10 | **Resonancia / Q** (suave → pico resonante que silba = épico) |

> La vibración de la tierra **suma** brillo encima del POT3 (cutoff base + energía → +5 kHz),
> por eso el dron se "abre" al pisar y se cierra en silencio.

## Modo de uso

1. Apoya el equipo en el piso y elige una escala (arranca en **NEBULOSA**).
2. El dron ya suena bajito: **pisa o golpea** el suelo cerca y escúchalo respirar/crecer.
3. Sube la **cola** (POT2) y abre **brillo/resonancia** (POT3/POT4) para texturas más épicas.
4. Sube el **desafine** (POT1) para un super-saw ancho y espacial.

## Ajuste de sensibilidad (en el `.ino`)

El piso, la distancia y la fuerza cambian cuánta vibración llega. Ajusta:

- `VIB_FULL` — *g* de vibración que abre el dron al máximo (más bajo = el dron respira con menos). Default `0.06`.
- `HIT_HIGH` — umbral para **sembrar** una nota-pad (más bajo = más sensible). Default `0.05`.
- `HIT_LOW` — umbral de rearme (histéresis). Default `0.02`.
- `HIT_MAX` — *g* que mapea a velocidad máxima de la nota. Default `0.60`.
- `TRIG_MIN_MS` — separación mínima entre notas sembradas. Default `120`.

> Como usa ±2g (4× más sensible que `impact_chimes`, que va en ±8g), reacciona a temblores
> mucho más leves. Si responde de más, **sube** `VIB_FULL`/`HIT_HIGH`; si responde de menos, bájalos.
> Tip: pegar el equipo bien firme al piso mejora la transmisión.

## Diagnóstico (sin USB)

- **El dron de base suena al encender** (tónica+quinta tenue) → firmware y audio OK. Si NO
  suena nada, el problema no es el IMU (revisa que la subida se haya completado).
- **El dron pulsa fuerte cada ~1.2 s** → **el IMU no se detecta**. Revisa el MPU6050
  (SDA 21, SCL 38, 3.3V, GND). Autodetecta direcciones **0x68 y 0x69**.

## Compilar y flashear (Arduino IDE)

- Board: **ESP32S3 Dev Module** · USB CDC On Boot: **Enabled** · Flash Mode: **DIO** · PSRAM: **OPI PSRAM**
- Librerías: ESP32 Arduino core ≥ 3.x (incluye `driver/i2s_std.h`) + `Wire.h` (incluida)
