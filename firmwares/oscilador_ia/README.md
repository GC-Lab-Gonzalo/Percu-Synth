# OSCILADOR IA

El sample de ElevenLabs **es el oscilador**. Pides un sonido hablando ("una cuerda metálica oxidada") y el PercuSynth le pide a ElevenLabs **ese sonido, tal cual**, traducido claro al inglés. El sample se convierte en la onda principal de un sintetizador afinado: teclado por grados de la escala, arpegiador, drones infinitos y secuencia melódica generativa.

## Por qué queda afinado de verdad

**A ElevenLabs no se le pide afinación ninguna** — es un generador de sonidos, no sabe de notas ni de Hz. La afinación vive completa en el firmware. Al cargar el sample:

1. **Detección de pitch por autocorrelación** (con guardia de error de octava + refinado parabólico): se mide la fundamental *real* del sample, caiga donde caiga. Todas las notas se calculan contra esa medición — el La sale La aunque el sonido haya llegado en 187 Hz.
2. **Sustain loop con crossfade horneado**: se toma la zona sostenida (45%–92% del sample), se anclan los bordes a cruces por cero y se hornea un crossfade en la costura. Una nota mantenida suena **para siempre** sin clicks = el sample funciona como oscilador continuo.

Si el sample no tiene periodicidad clara (ruido, textura), se asume 110 Hz: los intervalos entre notas siguen siendo correctos aunque la afinación absoluta quede a ciegas.

El prompt a ElevenLabs es **solo tu sonido**: GPT lo traduce claro al inglés y no se agrega nada más (ni afinación, ni "sustained", ni "loop" — las coletillas confunden al generador). La duración (4 s) y el flag de loop van como parámetros de la API, no como texto.

## Controles

| Control | Función |
|---|---|
| **BTN1** | Mantener = grabar pedido de timbre · toque corto = siguiente **escala** (10) |
| **BTN2–BTN5** | Según el modo (ver abajo) |
| **POT1** | BPM del arpegiador / secuencia (60–200) |
| **POT2** | Volumen master |
| **POT3** | **Tónica** (12 zonas: C2..B2) |
| **POT4** | **Modo** (6 zonas): TECLADO · ARP↑ · ARP↓ · ARP↕ · ARP aleatorio · SECUENCIA |
| **IMU** | X → cutoff · Y → resonancia |

### Modo TECLADO
BTN2–5 = fundamental / 3ª / 5ª / octava del acorde de la tónica. Toque = nota mientras presionas. **Mantener >0.6 s = DRONE infinito** (otro toque lo apaga). Puedes apilar hasta 4 drones = acorde-oscilador sostenido.

### Modos ARP
Cada botón **latchea** el arpegio sobre un acorde de la escala (grados I / IV / V / VI). Mismo botón = parar; otro botón = cambio de acorde al vuelo. Cambiar entre zonas ARP del POT4 cambia el patrón sin cortar el arpegio.

### Modo SECUENCIA
Melodía generativa de 4 compases: progresión modal (vocabulario de grados por escala + cadencia) y melodía que se ancla a notas del acorde en los pasos fuertes.
- **BTN2** play/stop · **BTN3** nueva melodía · **BTN4** nueva progresión · **BTN5** pedal de drone en la tónica.
- Cambiar de escala con BTN1 **reinterpreta la misma secuencia** en el modo nuevo (son grados, no notas).

## Escalas (10)

Jónico · Dórico · Frigio · Lidio · Mixolidio · Eólico · Locrio · Menor armónica · Frigio dominante · Doble armónica.

## LEDs (6 SMD)

| LED | Significado |
|---|---|
| 0 | Instrumento (color del sample, flash por nota; respira blanco si está vacío) |
| 1 | Modo (cian teclado · violetas arp · naranja seq; respira si corre) |
| 2 | Escala (flash al cambiarla) |
| 3 | Pulso (negra) |
| 4 | Filtro IMU |
| 5 | Estado: verde listo · rojo grabando · ámbar procesando · magenta error |

## Requisitos

- ESP32-S3 **con PSRAM** (OPI PSRAM en el IDE), Flash Mode **DIO**, USB CDC On Boot Enabled.
- FastLED. WiFi + keys de OpenAI y ElevenLabs (mismas de `sampler_ia`) en `secretos.h`: copia
  `secretos.example.h` a `secretos.h` en esta carpeta. Ese archivo está en el `.gitignore`.
- Plan ElevenLabs: pide `pcm_22050` y cae solo a `ulaw_8000` (Starter).

## Serial (115200)

Muestra el pedido transcrito, el prompt de timbre, el **pitch medido en Hz y cents respecto a A2**, los puntos del sustain loop, y todos los cambios musicales (escala, tónica, modo, progresión).
