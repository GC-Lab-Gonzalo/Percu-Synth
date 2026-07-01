# Tilt Maze — Laberinto de inclinación (PercuSynth)

Juego de laberinto de bola controlado **inclinando el PercuSynth** (IMU MPU6050).
Inspirado en los juguetes de madera de bolita + laberinto, pero digital, colorido y con
niveles progresivos. Hecho para GC Lab Chile.

## Cómo se juega

1. Carga el firmware en el PercuSynth (botón **⬇ Firmware (.ino)** en la pantalla de inicio).
2. Abre `index.html` en **Chrome o Edge de escritorio** (Web Serial).
3. Pulsa **Conectar PercuSynth** y elige el puerto USB.
4. Inclina la caja para rodar la bola hasta la **meta verde** (★).

Sin hardware se juega con teclado: **W A S D / flechas** inclinan, **R** reinicia el nivel,
**C** calibra el "cero" (la posición neutra del sensor).

## Reglas

- **Meta (★ verde):** llegar a ella completa el nivel y suma puntaje (más rápido = más puntos,
  + bonus por vidas restantes).
- **Hoyos (círculos oscuros):** si caes, pierdes una vida y vuelves al inicio del nivel.
- **Zonas rojas:** tocarlas quita una vida.
- **Portales (anillos giratorios de color):** te teletransportan a su par del mismo color.
- **Vidas:** empiezas con 3 (♥♥♥). Al quedarte sin vidas → Game Over.
- **Puntaje y récord:** cronómetro por nivel; el récord se guarda en el navegador.

## Niveles

10 niveles **curados a mano** con dificultad creciente, que introducen las mecánicas de a poco
(muros → hoyos → pasillos → portales → zonas mortales → combinaciones).

Cada nivel está **garantizado solucionable**: un validador interno (BFS sobre el piso, evitando
hoyos y zonas mortales, considerando los portales como conexiones) confirma que existe una ruta
segura inicio→meta. Si algún nivel no tuviera solución, lo avisa por consola. Los 10 niveles
pasan la validación.

### Formato de los mapas (para editar/añadir niveles)

Cada nivel es un arreglo de strings (una fila por string), con estos tiles:

| Símbolo | Significado |
|---|---|
| `#` | pared |
| `.` | piso |
| `S` | inicio (uno por nivel) |
| `G` | meta (una por nivel) |
| `O` | hoyo (caes → pierdes vida) |
| `X` | zona mortal (tocar → pierdes vida) |
| `p` `q` `r` | pares de portal (cada letra debe aparecer **exactamente 2 veces**) |

Reglas de diseño: filas del mismo largo, exactamente un `S` y una `G`, portales en pares, y debe
existir una ruta de piso (o vía portales) de `S` a `G` sin pisar `O` ni `X`.

## Técnica

- Un solo `index.html` autocontenido, sin build step. Canvas 2D + `requestAnimationFrame`.
- Lee el PercuSynth por **Web Serial** (CSV `A,accelX,accelY,...` a 115200, ~60 Hz).
- Firmware de telemetría IMU **embebido y descargable** desde el propio juego
  (`percusynth_imu_telemetry.ino`).
- Física de bola: aceleración por inclinación + fricción + colisión círculo-AABB contra las
  paredes (integración subdividida para no atravesarlas a alta velocidad).
- Jugable también con teclado (sin hardware) para demos y pruebas.
