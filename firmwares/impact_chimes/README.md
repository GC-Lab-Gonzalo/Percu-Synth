# impact_chimes — Campanas por golpe en el piso

Instrumento por **impacto** para el PercuSynth: se apoya el equipo en el **piso** y, al
golpear el piso cerca, la vibración llega al **acelerómetro** como un pico de aceleración
→ se dispara una nota dentro de la escala activa. Las notas se eligen con una *caminata
melódica* suave (siempre dentro de la escala → suena agradable) y las voces resuenan con
cola y se solapan, creando una textura tipo **campanas**.

- Usa el **acelerómetro** (no el giroscopio): es el sensor que capta golpes/vibración.
  Se descarta la gravedad con una línea base lenta, así solo los **golpes** disparan
  (tenerlo quieto o inclinado no suena).
- Motor: I2S → PCM5102 **44.1 kHz / 16-bit estéreo**, voces polifónicas con oscilador
  *morphing* (seno → triángulo → sierra), filtro paso-bajos y soft-limiter.
- Escala por defecto: **Eólica** (menor natural).

## Controles

### Golpe (acelerómetro)
Apoya el equipo en el piso y golpea el piso cerca → suena una nota. Golpe más fuerte =
nota más fuerte. Golpear seguido genera una secuencia.

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

## Modo de uso

1. Apoya el equipo en el piso y elige una escala con un botón (arranca en Eólica).
2. Golpea el piso cerca del equipo: cada golpe dispara una nota.
3. Sube el **Decay** (POT2) para colas largas; baja el **Ataque** (POT1) para notas secas.

## Ajuste de sensibilidad (en el `.ino`)

El piso, la distancia y la fuerza del golpe cambian cuánta vibración llega. Si no dispara
o dispara solo, ajusta:

- `HIT_HIGH` — umbral de disparo en *g* (más bajo = más sensible). Default `0.12`.
- `HIT_LOW` — umbral de rearme (histéresis). Default `0.05`.
- `HIT_MAX` — *g* que mapea a volumen máximo. Default `1.20`.
- `TRIG_MIN_MS` — separación mínima entre notas (sube para menos densidad). Default `45`.

> Si el piso transmite poca vibración, baja `HIT_HIGH` (p. ej. `0.06`) y prueba.
> Tip: pegar el equipo bien firme al piso mejora la transmisión del golpe.

## Diagnóstico (sin USB)

El firmware avisa con **sonido** qué está pasando, sin necesidad de Monitor Serie:

- **Acorde al encender** → el firmware y el audio funcionan en esa unidad. Si NO suena
  ese acorde, el problema no es el IMU (revisa que la subida se haya completado).
- **Nota grave repetida cada ~1.2 s** (latido) → **el IMU no se detecta**. Revisa el
  MPU6050 (cableado/soldadura: SDA 21, SCL 38, 3.3V, GND).
- El firmware **autodetecta la dirección I2C 0x68 y 0x69** (por si el pin AD0 quedó en
  alto en esa placa, que era una causa típica de "no responde").

Con esto, en una unidad sabes al instante: ¿suena el acorde? → audio OK. ¿hay latido? →
IMU mal. ¿acorde OK y sin latido pero no dispara al golpear? → es sensibilidad (`HIT_HIGH`).

## Compilar y flashear (Arduino IDE)

- Board: **ESP32S3 Dev Module** · USB CDC On Boot: **Enabled** · Flash Mode: **DIO** · PSRAM: **OPI PSRAM**
- Librerías: ESP32 Arduino core ≥ 3.x (incluye `driver/i2s_std.h`) + `Wire.h` (incluida)
