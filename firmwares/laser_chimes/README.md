# laser_chimes — Campanas al cortar el haz de un láser

Hermano de [`impact_chimes`](../impact_chimes/) con el **sensor de entrada cambiado**: donde
aquel escuchaba golpes en el piso con el acelerómetro, este vigila un **haz de láser** apuntado
a un **LDR**. Cada vez que algo cruza el haz (una mano, una baqueta, alguien bailando) la luz
que llega al LDR cae → se dispara una nota dentro de la escala activa.

- El motor de sonido es el mismo: I2S → PCM5102 **44.1 kHz / 16-bit estéreo**, voces
  polifónicas con oscilador *morphing* (seno → triángulo → sierra), filtro paso-bajos y
  soft-limiter. Las notas siguen una **caminata melódica** dentro de la escala, así que
  cualquier secuencia de cortes suena bien.
- El LDR se muestrea a **1 kHz en una tarea propia clavada en el core 0**, así el corte se
  detecta con ~1 ms de resolución sin robarle tiempo al audio (core 1). La cola de DMA es
  corta (4×128 ≈ 12 ms) para que la nota se oiga en el acto.
- **No usa el IMU** ni FastLED: sin librerías externas.
- Escala por defecto: **Eólica** (menor natural).

## Conexión

El LDR va al **sensor externo A (EXT1) = GPIO 3** formando un divisor de tensión con la
resistencia de 220 Ω. Sirve en cualquiera de las dos formas:

```
3.3V ── LDR ──┬── EXT1 (GPIO 3)        con láser = lectura ALTA
              220 Ω
              GND

3.3V ── 220 Ω ──┬── EXT1 (GPIO 3)      con láser = lectura BAJA
                LDR
                GND
```

El firmware **no asume la polaridad**: se define escribiendo las dos lecturas reales en
`LDR_LASER` y `LDR_TAPADO` (si la primera es menor que la segunda, el mapeo se invierte solo).

El láser **no se conecta a la placa** — es una fuente de luz independiente apuntando al LDR.

> ⚠️ Un láser verde apunta a la altura de los ojos con demasiada facilidad. Móntalo apuntando
> hacia abajo o contra una pared, nunca a la altura de la cara del público.

## Puesta a punto (lo único que hay que ajustar)

En el `.ino`, bloque **AJUSTE DEL LDR**:

| Variable | Qué es | Default |
|---|---|---|
| `LDR_LASER` | Lectura del ADC (0–4095) **con el láser** dando de lleno en el LDR | `3200` |
| `LDR_TAPADO` | Lectura del ADC con el haz **interrumpido** (luz ambiente sola) | `600` |
| `UMBRAL_CORTE` | Fracción del recorrido por debajo de la cual se considera cortado | `0.45` |
| `UMBRAL_REARME` | Hay que volver por encima de esto para poder disparar de nuevo (histéresis) | `0.65` |
| `TRIG_MIN_MS` | Tiempo muerto mínimo entre notas (ms) | `60` |
| `LDR_SUAVIZADO` | Filtro de la lectura: `1.0` = crudo, `0.2` = muy suave | `0.60` |
| `VEL_POR_PROFUNDIDAD` | Volumen según lo profundo del corte (`false` = todos igual de fuertes) | `true` |
| `VEL_MINIMA` | Volumen del corte más leve (0..1) | `0.40` |
| `MOSTRAR_ESTADO` | Diagnóstico por Serial a 115200 (ponlo en `0` una vez calibrado) | `1` |

Los umbrales van como **fracción** del recorrido entre `LDR_TAPADO` (0.0) y `LDR_LASER` (1.0),
no en cuentas del ADC, para que cambiar los dos números de arriba no obligue a recalcular nada
más.

**Cómo medir los dos números:**

1. Flashea con `MOSTRAR_ESTADO 1` y abre el Monitor Serie a **115200**.
2. Con el láser dando en el LDR, mira `raw` → ese número va en `LDR_LASER`.
3. Tapa el haz con la mano, mira `raw` → ese número va en `LDR_TAPADO`.
4. Escribe los dos valores y vuelve a flashear.

La línea del monitor trae además `min`/`max` vistos desde el arranque, así que cruzando el haz
unas cuantas veces los dos extremos quedan a la vista sin tener que congelar nada:

```
LDR raw=3187  luz=0.99  [HAZ OK]        min=412 max=3241   (LDR_TAPADO=600 LDR_LASER=3200)
>> CORTE  raw=489  luz=0.03  vel=0.94  grado=6
```

- Si **dispara solo**: separa más `LDR_LASER` de `LDR_TAPADO`, o sube `UMBRAL_CORTE`, o baja
  `LDR_SUAVIZADO` (más filtro = menos ruido del ADC).
- Si **un corte suena dos veces**: sube `TRIG_MIN_MS` o baja `UMBRAL_REARME`.
- Si el rango entre láser y tapado es muy chico, prueba otra resistencia: con 220 Ω el divisor
  favorece al LDR muy iluminado; para más recorrido con luz ambiente alta va mejor 1 k–10 k.

## Controles

### Corte del haz (LDR)

Cruza el haz → suena una nota. Corte más profundo (tapar el haz entero) = nota más fuerte;
rozarlo apenas = nota más suave. La nota se dispara en el **flanco de corte** (al interrumpir),
no al restablecer el haz.

### Escala — un botón por escala (queda fija hasta que cambies)

| Botón | Pin | Escala |
|---|---|---|
| BTN1 | 44 | **Eólica** (menor natural) — *por defecto* |
| BTN2 | 42 | Mayor (jónica) |
| BTN3 | 0  | Dórica |
| BTN4 | 45 | Pentatónica menor |
| BTN5 | 47 | Pentatónica mayor |

### Síntesis — potenciómetros

| Pot | Pin | Función |
|---|---|---|
| POT1 | ADC1 | **Ataque** (0.5 ms percusivo → ~150 ms suave) |
| POT2 | ADC2 | **Decay** / cola (~0.15 s corto → ~5 s tipo pad/campana) |
| POT3 | ADC8 | **Brillo** (cutoff del filtro paso-bajos) |
| POT4 | ADC10 | **Timbre** (morphing de onda: seno → triángulo → sierra) |

## Diagnóstico

- Al encender suena un **acorde de arranque** → confirma que el firmware y el audio funcionan
  en esa unidad. Si no lo escuchas, el problema **no** es el sensor.
- El diagnóstico por Serial sale por **los dos puertos** (`Serial` y `Serial0`): la DevKitC-1
  tiene dos conectores USB y *USB CDC On Boot* decide cuál es `Serial`. Si no ves nada, prueba
  el otro cable antes de dar por roto el firmware.

## Ajustes del IDE

- Board: **ESP32S3 Dev Module**
- USB CDC On Boot: **Enabled**
- Flash Mode: **DIO** (¡OPI rompe el I2S!)
- PSRAM: OPI PSRAM
