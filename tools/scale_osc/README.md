# Scale OSC — Motor de tono cuantizado en el navegador

Webapp standalone (un solo `index.html`, sin build) donde **cada potenciómetro es un oscilador
cuantizado a una escala**. Es la versión de navegador de la misma idea que el firmware
[`oscilador_escalas`](../../firmwares/oscilador_escalas/): muevas donde muevas la perilla, la nota
que sale siempre está en la escala.

Sirve para dos cosas: probar el concepto **sin flashear nada**, y usarla como instrumento en vivo
con el PercuSynth de controlador.

## Controles

- **SCALE** — mayor, menor, menor armónica, pentatónicas y el resto del set de la casa
- **ROOT** — tónica
- **OCTAVE** — octava base (también con las teclas `Z` / `X`)
- **OSCILLATOR · FILTER · ENVELOPE · REVERB** — la cadena de síntesis

## Conexión con el hardware

Se conecta al PercuSynth por **Web Serial** (botón CONECTAR): los 4 pots pasan a manejar los 4
osciladores y el **TILT** del IMU barre el filtro. También acepta un controlador por **Web MIDI**.
Sin hardware conectado funciona igual con el mouse y el teclado.

Requiere **Chrome o Edge** (Firefox y Safari no soportan Web Serial ni Web MIDI).
