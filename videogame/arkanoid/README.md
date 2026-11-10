# Arkanoid — PercuSynth

El clásico de romper ladrillos, controlado con el PercuSynth. Cada cristal que rompes suena
una nota de **La menor pentatónica**.

## Cómo se juega

1. Carga el firmware en el PercuSynth (botón **Firmware .ino** en la pantalla de inicio). Es el
   mismo de Tilt Maze: si ya lo tienes flasheado, no hay que hacer nada.
2. Abre `index.html` en **Chrome o Edge de escritorio** (Web Serial).
3. Pulsa **Conectar PercuSynth** y elige el puerto USB. La partida empieza sola.

| Control | PercuSynth | Teclado |
|---|---|---|
| Paleta a la izquierda (mantener) | BTN1 | ← o A |
| Paleta a la derecha (mantener) | BTN5 | → o D |
| Lanzar la bola / disparar / continuar | BTN3 | espacio |

La paleta dispara dos balas hacia arriba (una por cada boca), con 0,28 s entre disparos.

## La música

La nota de cada cristal la decide **su columna**. Las 10 columnas son exactamente dos octavas de
la pentatónica menor desde A3:

| Columna | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|
| Nota | A3 | C4 | D4 | E4 | G4 | A4 | C5 | D5 | E5 | G5 |

Grave a la izquierda, agudo a la derecha: el tablero es también un teclado. Al pasar de nivel suena
la escala entera hacia arriba. Los golpes que no rompen (trizar un cristal negro, pegarle a un bloque
fijo) dan sólo un toque seco de ruido, sin nota.

La voz es un pluck: dos sierras desafinadas ±7 cents y un triángulo, por un pasa-bajos que se cierra
de 9·f a 1,6·f, con un eco fijo de 0,27 s y un compresor al final.

## Cristales y niveles

| Símbolo | Qué es |
|---|---|
| `r` `b` `y` | cristal de un golpe (rojo, azul, amarillo) — 10 puntos |
| `k` | cristal negro de dos golpes; el primero lo triza — 20 puntos |
| `x` | bloque rayado que no se rompe (no cuenta para pasar el nivel) |
| `.` | vacío |

Los niveles están en `NIVELES` dentro de `index.html`, una fila por string de 10 caracteres.
Hay 6; después del último vuelven a empezar con la bola más rápida. Se empieza con 3 vidas y el
récord se guarda en el navegador.

## Técnica

- Un solo `index.html` sin build step: Canvas 2D + Web Audio + Web Serial.
- Protocolo: `A,accelX,accelY,accelZ,gyroX,gyroY,gyroZ,pot1,pot2,pot3,pot4,btn1,btn2,btn3,btn4,btn5`
  a 115200 baudios, ~60 líneas por segundo. BTN3 se toma en el flanco de presión.
- El audio se habilita con el clic en **Conectar** o **Jugar**: el navegador no deja sonar nada
  sin un gesto en la página, y apretar un botón de la placa no cuenta como uno.
- Física de la bola subdividida en pasos de ~3 px para que no atraviese cristales; el ángulo de
  salida depende de dónde pega en la paleta (±60°).
