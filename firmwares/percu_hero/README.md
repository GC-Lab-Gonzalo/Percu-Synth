# Percu Hero — en la placa

El juego de ritmo de [`videogame/percu_hero`](../../videogame/percu_hero/), pero **entero en el
PercuSynth**: la canción se sintetiza y sale por el DAC, las notas vienen hacia ti por la OLED y los
5 botones son los 5 carriles. Sin computador.

Por ahora trae una canción, **«Turno de noche»** (Re hijaz, un acorde, 128 BPM).

## Qué necesita

- PercuSynth V2.0 con la **OLED SSD1306** en su conector (horizontal, como viene montada).
- Audífonos o parlante en la salida del DAC.
- Librería **FastLED**. Un solo archivo: `percu_hero.ino`.

Arduino IDE: *ESP32S3 Dev Module*, USB CDC On Boot *Enabled*, Flash Mode **DIO**.

## Cómo se juega

| Momento | Botón | Qué hace |
|---|---|---|
| Menú | BTN1 / BTN5 | dificultad anterior / siguiente (FÁCIL · NORMAL · DURO) |
| Menú | BTN3 | empezar |
| Jugando | BTN1 … BTN5 | los cinco carriles, de izquierda a derecha |
| Al terminar | BTN3 | otra vez |
| Al terminar | BTN1 / BTN5 | volver al menú |

Aprieta el botón de cada nota cuando llega a su casilla. Las notas con cola se mantienen apretadas
hasta que termina la cola.

- **La base suena siempre; la melodía sólo si aciertas.** Va siempre en la grilla: mientras vienes
  acertando, cada nota entra exacta sobre la base; al fallar se corta y se calla hasta el próximo acierto.
- **Energía** (10 bloques a la izquierda): cada falla quita, cada acierto devuelve, más con PERFECTO y
  con el multiplicador alto. En cero se acaba la partida.
- **Multiplicador**: sube cada 10 aciertos seguidos, hasta ×4.

## La pantalla

La pista va en perspectiva: las notas nacen en el horizonte y vienen hacia ti. Detrás pasa la
galaxia en tres formas que se turnan cada 4 compases — hiperespacio de trazos, planetas y túnel de
anillos hexagonales — a la velocidad de la energía de la sección, con un salto en cada bombo.

En las **zonas difíciles** (muchas notas por compás) hay más estrellas, se suma el hiperespacio,
cruzan asteroides, los bordes de la pista engordan y la pantalla golpea con el bombo.

Arriba: multiplicador y racha a la izquierda, el juicio del último golpe al centro (PERF · BIEN ·
MAL) y el puntaje a la derecha.

LEDs: los cinco primeros marcan el botón apretado (crema) y el acierto (ámbar), rojo al fallar. El
sexto late con el bombo; **magenta** = un bloque de audio se pasó del 80 % de su tiempo.

## Cómo está hecho

- **Un solo dueño del juego.** El audio (core 1) lleva el reloj, el secuenciador y todo el estado del
  juego. Los botones se leen a 1 kHz en el core 0 y sólo dejan eventos con la hora exacta del toque
  (la muestra de la canción que se estaba oyendo); la pantalla sólo lee.
- **La música es texto**, igual que en el juego web: `BAT`, `BAJO`, `SECCIONES` (forma, energía y
  chart) y `FONDOS`.
- **Motor**: bombo de tres bandas con su saturación, percusión de ruido pasa-banda, bajo monofónico
  (sierra + cuadrada → saturación → SVF), melodía en cuatro timbres (STAB, ACERO, FM, QUINTAS) con eco
  ping-pong, dron y subida en los quiebres, sidechain, pasa-bajos de toda la base que se abre con la
  energía, limitador con anticipación. No lleva reverb (el juego web sí).
- **Medido en la placa** (`-DMEDIR=1`: la canción parte y se toca sola e imprime el uso por los dos
  USB): 55–66 % del tiempo de bloque en promedio, con picos aislados de un bloque cerca de 100 % que
  absorbe la cola de 12 ms. Llegar ahí pidió tres cosas, que valen para cualquier firmware:
  - **nada de `double` por muestra** (el S3 no tiene FPU de doble precisión): comparar el reloj con
    el próximo paso en `double` costaba un tercio del bloque;
  - **la melodía es una línea**: la nota nueva despide a las anteriores en 30 ms. Con colas de 2 s la
    reserva de voces vivía llena;
  - **los coeficientes de cada golpe se calculan una vez** y se guardan (`expf`, `powf`, `sinf` en
    cada disparo son caros), y la saturación es una cúbica sin división.
- `LATENCIA_MS` (13) es lo que tarda en oírse lo que se rinde; `PANTALLA_MS` (12) lo que tarda un
  cuadro en llegar a la OLED. Si las notas se sienten adelantadas o atrasadas, se ajusta ahí.

## Estado

Escrito y medido el 2026-11-10. Compila, corre en la placa y el uso de procesador está medido; **no
se simuló en el PC** y el sonido, la vista y la dificultad todavía no han sido revisados jugando.
