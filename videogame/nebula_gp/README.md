# NEBULA GP — Simulador de carreras FPV de drones (PercuSynth)

Videojuego de carreras FPV en primera persona para el **PercuSynth**: 3 vueltas contra
**4 drones bot** en un circuito nocturno de neón, con humo flotando sobre la pista y un
cielo galáctico (nebulosas de colores, banda galáctica, planeta anillado y estrellas
titilantes). Webapp de un solo archivo (`index.html`), sin build ni dependencias.

## Cómo se juega

| Control | Acción |
|---|---|
| **IMU** inclinar izq/der | **Rotación** (yaw) |
| **IMU** inclinar adelante/atrás | **Cabeceo** (pitch) = descender / elevarse |
| **BTN5** | Avanzar (acelerador) |
| **BTN1** | Retroceder / freno |
| **BTN2** | Desplazamiento lateral izquierda |
| **BTN4** | Desplazamiento lateral derecha |
| **BTN3** | Recentrar IMU (en el menú: iniciar carrera) |
| **POT1** | Volumen |
| **POT2** | Sensibilidad del IMU |

> El mapeo vive en constantes al inicio del JS
> (`BTN_FWD`, `BTN_BACK`, `BTN_LEFT`, `BTN_RIGHT`, `BTN_UTIL`) — trivial de cambiar.

**Sin hardware:** mouse = yaw/pitch · `W`/`↑` avanza · `S`/`↓` retrocede · `A`/`D`
laterales · `Z` recentrar IMU · `M` sonido · `Enter` iniciar.

## La carrera

- Circuito cerrado (spline Catmull-Rom con colinas) delimitado por **bordes de neón**
  (cyan a la izquierda, magenta a la derecha) y **16 arcos**; el dorado es la **META**.
- **4 bots** (VOLT, NYX, KAJI, ORO) con velocidades propias y *rubber-banding* suave.
- Salirse del corredor de energía frena el dron (viñeta roja + aviso); un muro elástico
  invisible impide perderse del todo.
- HUD: posición, vuelta, tiempo de vuelta y mejor vuelta, velocímetro, **minimapa** con
  todos los corredores, indicadores de botones y estado del Web Serial.
- Sonido 100% sintetizado (Web Audio): motor (2 saw detuned + LPF), viento por
  velocidad, beeps de largada, whoosh al cruzar arcos, fanfarria final.

## Conexión con el PercuSynth

1. Flashea el firmware: botón **⬇ FIRMWARE .ino** dentro del juego
   (`nebula_gp_control_percusynth.ino`). Es el **mismo protocolo que NEON STRIKE**
   (`p0,p1,p2,p3,b1,b2,b3,b4,b5,imuX,imuY` @ 115200, 50 Hz) — si ya tienes el firmware
   de cyber_flight flasheado, sirve tal cual.
2. Abre el juego en Chrome/Edge sobre `http://localhost` (Web Serial exige contexto
   seguro). Por ejemplo: `python -m http.server 8776` en esta carpeta.
3. Botón **🔌 CONECTAR PERCUSYNTH** → elegir el puerto USB **nativo** del ESP32-S3.
4. El primer paquete auto-centra el IMU; recentra cuando quieras con **BTN5** (o `Z`).

## Arquitectura (index.html)

- **Render 3D propio sobre Canvas 2D**: proyección perspectiva manual + painter's
  algorithm. La pista se dibuja como quads a lo largo del spline (58 segmentos hacia
  adelante); arcos/pilones/humo/bots se ordenan por profundidad.
- **Cielo**: panorama pre-renderizado (2048×760) que se desplaza con yaw/pitch —
  nebulosas aditivas, banda galáctica, planeta con anillos, ~850 estrellas + 14
  titilantes dibujadas en vivo. Grilla de suelo synthwave con clipping en cámara.
- **Humo**: 64 sprites de niebla (gradientes radiales aditivos) anclados a la pista,
  que derivan lentamente y se atenúan al atravesarlos.
- **Física arcade**: aceleración/drag, strafe, elevación por pitch, techo/piso relativos
  a la pista, choque suave contra bots, progreso de vuelta por arco continuo (tolera
  retrocesos, el mute nunca desincroniza el conteo de vueltas).
- **Bots**: siguen la línea central con offset lateral oscilante, velocidad base
  37–43.5 u/s y rubber-banding (±15% según distancia al jugador).
