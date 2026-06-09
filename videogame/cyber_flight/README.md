# cyber_flight — NEON STRIKE (shooter cyberpunk en 1ª persona)

Webapp de una página (`index.html`, sin build) — un **simulador de combate aéreo
cyberpunk** sobre una megaciudad distópica. Pilotas un caza en primera persona y
derribas naves enemigas que se acercan desde el cielo neón. Controlado por la
**PercuSynth** (botones + IMU) o, sin hardware, con **mouse + teclado**.

## Uso

1. Abre `index.html` en **Chrome o Edge** (Web Serial).
2. Pulsa **▶ INICIAR MISIÓN** (necesita un clic para activar el audio).
3. (Opcional) **⬡ CONECTAR PERCUSYNTH** y elige el puerto del **USB nativo** del
   ESP32-S3. Sin hardware: usa mouse/teclado.

> Servir local: `python -m http.server 8772 --directory tools/cyber_flight`.

## Controles (PercuSynth)

| Control | Acción |
|---------|--------|
| **Inclinar izquierda / derecha** | Mueve el caza / la mira **◄ ►** (der → mira a la der) |
| **Inclinar adelante / atrás** | Mueve el caza / la mira **▲ ▼** (adelante → abajo, atrás → arriba) |
| **BTN5** | Gatillo **derecho** — las balas salen desde la **derecha** |
| **BTN1** | Gatillo **izquierdo** — las balas salen desde la **izquierda** |
| **BTN2 + BTN4** (juntos) | **BOMBA** — arma destructiva que limpia la pantalla |

La **mira está fija al centro**: apuntas moviendo el caza con el IMU hasta que el
enemigo queda bajo el retículo (aparece **LOCK**). Mantener un gatillo dispara en
ráfaga. La bomba necesita carga 100 % (se recarga sola) y al detonar destruye todo
lo visible con onda expansiva + cámara lenta.

## Teclado / mouse (probar sin hardware)

`mouse` apuntar · `click izq`/`A`/`1` gatillo izq · `click der`/`L`/`5` gatillo der ·
`espacio`/`B` bomba · `Z` centrar IMU · `M` música.

## Mecánica

- **3 sectores (etapas)** con final: `DISTRITO NEÓN` → `SECTOR ÁCIDO` → `NÚCLEO
  INFERNAL`. Cada sector cambia por completo el **escenario** (cielo, sol, ciudad,
  grid y color del HUD) y termina con un **GRAN JEFE**. Derrotar al jefe final =
  pantalla de **¡VICTORIA!**. El juego ya **no es infinito**.
- **Oleadas** crecientes dentro de cada sector (más enemigos y tipos más duros:
  drone → striker → hunter).
- **Jefes**: nave enorme con barra de vida propia arriba, que se mantiene a
  distancia barriendo de lado a lado y **lanza orbes que persiguen tu mira** — los
  puedes **derribar a tiros** antes de que te impacten.
- Los enemigos se mantienen **siempre dentro del rango alcanzable** de la mira (ya
  no se escapan fuera de cuadro).
- **Puntaje** con **multiplicador** que sube en cada derribo y se reinicia al fallar
  un disparo o recibir daño.
- **Integridad** (HP): baja cuando una nave te alcanza. A 0 → fin de misión.

## Efectos dinámicos

Cambio de escenario por sector, líneas de **warp/hipervelocidad** desde el centro
(sensación de avance, se intensifican en los saltos de sector), **alabeo (banking)**
de la cámara al apuntar, banners de sector/jefe, alarma + flash en la aparición del
jefe, pulso rojo de pantalla con HP bajo, explosiones del jefe en cadena, cámara
lenta y aberración cromática en momentos clave.

## Gráfica / efectos

Todo en **Canvas 2D**, estética synthwave/cyberpunk: cielo en degradado, sol con
bandas, skyline neón parallax con ventanas, grid en perspectiva con scroll (sensación
de avance), estrellas titilantes. Efectos: glow aditivo, partículas/chispas, anillos
de explosión, fogonazos, *screen shake*, flash, aberración cromática (en la bomba),
*scanlines* y viñeta. SFX por **Web Audio** (láser, explosión, bomba, daño) + arpegio
synthwave opcional (`M`).

## Firmware PercuSynth (incluido)

Botón **⬇ FIRMWARE .ino** → `neon_strike_control_percusynth.ino` (embebido).
Protocolo USB-Serial **115200, 50 Hz**:

```
ENVIA: p0,p1,p2,p3,b1,b2,b3,b4,b5,imuX,imuY
```

- `p0..p3` pots (ADC 1/2/8/10), reservados · `b1..b5` botones (44/42/0/45/47) ·
  `imuX`/`imuY` = ejes X/Y del acelerómetro mapeados a 0..4095 (centro ~2048, EMA).
- IMU por **I2C crudo (`Wire`)**, sin librería externa (igual que `firmwares/test_imu`),
  autodetección 0x68/0x69. Flashea con **ESP32S3 Dev Module**, **USB CDC On Boot =
  Enabled**, **Flash Mode = DIO**.

### ⚠️ "Conectado pero SIN datos"

La PercuSynth tiene **dos puertos USB-C**: el del **chip UART** (para flashear) y el
**USB nativo** del ESP32-S3 (por donde sale el Serial). **Conecta la webapp al USB
nativo** (es un COM distinto al de programar). La píldora de estado se pone cian y
muestra **DATOS OK** cuando el puerto es el correcto.
