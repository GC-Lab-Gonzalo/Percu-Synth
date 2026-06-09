# matrix_midi_anyma

Máquina **audiovisual electro estilo Anyma** para el PercuSynth con la **matriz WS2812 20×20**. Tres cosas en un solo firmware:

1. **Secuenciador interno** de 16 pasos con 4 patrones techno/electro (kick a la negra, claps, hats rodando, bajo *acid* sincopado). Envía la base por **USB MIDI**: batería en **ch10** (GM drums) + bajo en **ch1** → pones tus instrumentos en el DAW.
2. **MIDI Clock Master**: emite reloj `0xF8` a 24 PPQ de forma continua + `0xFA`/`0xFC` (Start/Stop). Pon tu DAW en *External / MIDI Clock Sync* y queda pegado al tempo.
3. **Motor visual 2D** sobre la matriz con 5 escenas espectaculares que reaccionan **tanto al secuenciador interno como a notas MIDI entrantes** (un secuenciador externo también pinta la matriz).

El foco es **movimiento**, no solo color: blending aditivo (HDR), estela por *motion-blur*, sistema de partículas con física, ondas de choque radiales y *pump* de brillo bloqueado al kick — toda la matriz respira con el beat.

## Escenas (BTN1 cicla)

| Escena | Qué hace |
|---|---|
| **NEXUS** | Plasma fluido de fondo + partículas con estela. Kick = onda de choque + estallido; clap = anillo color complementario; notas = cohetes verticales. |
| **TÚNEL** | Espiral concéntrica infinita que *zooma* con cada kick. Hipnótico, muy techno. Notas = anillos de luz a su radio. |
| **ESPECTRO** | Columnas por nota (pitch→X, velocity→altura) con *caps* que caen; kick/clap llenan el espectro de golpe. |
| **TORMENTA** | Física pura de partículas con gravedad: kick = explosión central, clap = lluvia, notas = cohetes. |
| **REJILLA** | 5×5 celdas geométricas que laten con el beat + haz de barrido sincronizado al step. |

## Patrones (BTN2 cicla)

`DRIVE` (4×4 directo) · `ANYMA` (sincopado con ghosts + acid bass) · `PEAK` (build energético) · `BREAK` (kick roto / half-time).

## Controles

| Control | Acción |
|---|---|
| BTN1 (42) | Escena siguiente |
| BTN2 (44) | Patrón siguiente |
| BTN3 (45) | Play / Stop (envía Start/Stop MIDI) |
| BTN4 (47) | Impacto manual: crash + supernova visual |
| BTN5 (0)  | Blackout / pánico |
| POT1 (ADC1) | Brillo global (5–80) |
| POT2 (ADC2) | BPM (90–150) |
| POT3 (ADC8) | Color base / paleta (hue) |
| POT4 (ADC10) | Intensidad FX (estela + densidad de partículas + caos) |

## Cómo usarlo con un DAW

1. Conecta el PercuSynth → aparece como dispositivo MIDI.
2. DAW: **clock IN = External (MIDI Clock)**, y enruta **ch10** a tu sampler de batería y **ch1** a tu sinte de bajo.
3. Arranca: ya viene en PLAY con un groove. BTN2 cambia el patrón, BTN1 el show.
4. ¿Quieres que la matriz reaccione a *tu* secuencia del DAW? Manda NoteOn de vuelta al PercuSynth: ch10 dispara visuales por tipo de drum, cualquier otra nota → columnas/partículas.

## Hardware / settings

- ESP32-S3, matriz WS2812 20×20 en **DATA 46** (cableado progresivo, primeros 6 LEDs SMD internos apagados).
- Arduino IDE: **ESP32S3 Dev Module · USB CDC On Boot: Enabled · USB Mode: USB-OTG (TinyUSB) · Flash Mode: DIO · PSRAM: OPI**.
- Librería: **FastLED**.

## Notas técnicas

- Refrescar 406 LEDs bloquea ~12 ms por frame; el reloj MIDI usa un acumulador (`nextClockUs`) que recupera ticks atrasados, así el **tempo promedio** se mantiene exacto (puede haber leve jitter, que el PLL de clock del DAW absorbe).
- Mezcla **aditiva**: los colores se suman y saturan en blanco, por eso el plasma de fondo se mantiene tenue para que las reacciones destaquen.
- Si la matriz queda espejada, ajusta `FLIP_X` / `FLIP_Y` arriba del `.ino`.
