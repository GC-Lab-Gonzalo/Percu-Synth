# Bichos MIDI — insectos, grillos y aves por nota MIDI

Webapp standalone (un solo `index.html`, sin build, sin samples). Se abre, se hace **un clic para
activar el sonido** (el navegador lo exige) y se toca cualquier controlador MIDI: cada nota dispara
un sonido corto de insecto o ave sintetizado en el momento. Nada más.

## Uso

1. Abre `index.html` en **Chrome o Edge** (Firefox/Safari no tienen Web MIDI).
2. Acepta el permiso MIDI y haz clic en la pantalla.
3. Toca. Se escuchan **todas** las entradas MIDI a la vez, también las que se enchufan después, en
   cualquier canal. Sin controlador, los recuadros en pantalla también se pueden tocar con el mouse.

## Qué suena

La nota dentro de la octava elige el bicho; la octava cambia un poco su **tamaño** (más grave abajo,
más agudo arriba) y la velocidad, el volumen. Cada golpe varía levemente al azar para que dos golpes
seguidos no suenen idénticos.

| Nota | Sonido | | Nota | Sonido |
|------|--------|-|------|--------|
| Do  | Grillo           | | Fa#  | Escarabajo (chasquidos) |
| Do# | Grillo arbóreo   | | Sol  | Pío |
| Re  | Chicharra        | | Sol# | Silbido |
| Re# | Saltamontes      | | La   | Trino |
| Mi  | Abejorro         | | La#  | Gorjeo |
| Fa  | Mosquito         | | Si   | Cucú |

## Por qué ninguna nota se queda pegada

- **Cada sonido es un buffer de duración fija** (0,3 a 0,95 s, tope duro de **1 s** en el código)
  que ya trae su envolvente y termina en cero. No hay sustain: el **Note Off se ignora** a propósito,
  así que un Note Off perdido, un cable desconectado o un controlador que no lo manda no pueden dejar
  nada sonando.
- `src.stop()` se agenda en el mismo momento del disparo, y un vigilante cada 200 ms corta con
  fundido cualquier voz que pase de 1 s (no debería ocurrir nunca; está por si acaso).
- **Una voz por nota**: repetir la misma nota corta la anterior con un fundido de 8 ms, sin click.
  Máximo 16 voces; si se llena, se roba la más vieja con el mismo fundido.
- **SILENCIO** en pantalla, o CC 120 / CC 123 desde el controlador, corta todo.

Verificado en Chromium: los 12 sonidos en 9 octavas × 15 variaciones miden como máximo 0,95 s, 0
NaN, pico ≤ 1, empiezan y terminan en 0; una ráfaga de 200 notas con Note Off mezclados y 30
repeticiones de la misma nota deja **0 voces vivas** 1,4 s después.
