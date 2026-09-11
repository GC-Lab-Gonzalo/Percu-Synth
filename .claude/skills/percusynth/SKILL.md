---
name: percusynth
description: Crear, modificar y depurar firmware Arduino para el PercuSynth (ESP32-S3 + DAC PCM5102 + IMU MPU6050 + LEDs WS2812) y las webapps que lo controlan por Web Serial / Web MIDI. Úsala cuando el trabajo toque un .ino de este repositorio, el pinout de la placa, síntesis de audio a 44.1 kHz por I2S, o una herramienta de tools/ que hable con el hardware.
---

# PercuSynth — firmware y herramientas

Laboratorio de electrónica musical de GC Lab Chile: una placa ESP32-S3 con salida de audio I2S,
IMU, botones, pots, piezos y LEDs direccionables. El repositorio son **sketches Arduino
independientes** (`firmwares/`) y **webapps de un solo archivo** (`tools/`, `videogame/`).

## Antes de escribir código

Lee **[`PROMPT_PARA_LA_IA.md`](../../../PROMPT_PARA_LA_IA.md)** en la raíz del repositorio. Es el
documento madre: pinout fijo, ajustes del Arduino IDE, librerías, constantes de audio y los
patrones de código canónicos (init de I2S, botones con debounce, pots con oversampling, lectura
cruda del MPU6050, USB-MIDI, FastLED). Copia y adapta desde ahí en vez de improvisar.

Si vas a modificar un firmware que ya existe, **lee su `README.md` primero**: cada carpeta de
`firmwares/` y `tools/` tiene uno, y ahí están las decisiones de diseño que ya se tomaron y por
qué. Muchas son el resultado de un problema real que se midió — deshacerlas suele reintroducir el
bug.

## Lo que no se negocia

- **El pinout es fijo.** No inventes pines: están en `PROMPT_PARA_LA_IA.md` §2. Los botones son
  `{44, 42, 0, 45, 47}` en ese orden.
- **Flash Mode DIO**, nunca OPI: con OPI el audio se corta y cruje en este hardware.
- **Audio por I2S al PCM5102** a 44.1 kHz / 16-bit estéreo, bloques DMA de 128 muestras. Nada de
  `dacWrite()` ni del DAC interno.
- **Nunca `delay()` dentro del render de audio.**
- **LEDs:** los índices 0..5 son los SMD internos de la placa. Empieza a escribir en el 6 salvo
  que el firmware los use a propósito como indicadores de estado.
- **Con WiFi encendido el ADC2 no se puede leer.** Los 4 pots ya están en ADC1 (1, 2, 8, 10).
- **MIDI DIN-5 = GPIO 43 a 31250 baud**, y ese pin es también el TX del `Serial0`; el RX del
  UART0 es el 44, o sea el BTN1. En un firmware con DIN-5: usa `Serial1` con `rxPin = -1`, e
  imprime **sólo** por `Serial` (USB CDC) — un `Serial0.print` sale por el cable MIDI como
  basura. Referencia: `firmwares/drum_poder/`.
- **Credenciales fuera del `.ino`:** van en un `secretos.h` (ignorado por git) con su
  `secretos.example.h` al lado. Ver `PROMPT_PARA_LA_IA.md` §4.1.

## Convenciones del proyecto

- Comentarios y nombres de variables **en español**.
- Todo `.ino` empieza con el **encabezado formato proto-synth-v2**: bloques HARDWARE / ARDUINO IDE
  SETTINGS / LIBRERÍAS REQUERIDAS / DESCRIPCIÓN / FUNCIONAMIENTO separados por líneas de `====`.
  La plantilla exacta está en `PROMPT_PARA_LA_IA.md` §9.
- Las webapps son **HTML de un solo archivo**: CSS y JS inline, sin build, sin npm, sin CDN de
  librerías salvo fuentes.
- Cada firmware o herramienta nueva lleva su propio `README.md`.

## Diseño de controles

Estas reglas vienen de iteraciones con usuarios reales en talleres:

- **Un botón, una función**, y que se oiga **en el flanco de presión**. Nada de ventanas de espera
  para detectar combos: si necesitas combos, dispara igual al presionar y que el combo *deshaga*
  la acción individual con un fast-kill de ~4 ms.
- **Los pots significan siempre lo mismo** y su posición física *es* el valor. Los paneles
  congelados y el pick-up confunden; úsalos solo si el firmware ya los tiene.
- **No inventes paneles ni combos** que no se pidieron. Si hace falta un parámetro más, primero
  considera dejarlo como constante fija en el código.
- **Nada se mueve solo.** Los LFO que generan control autónomo sobre parámetros que el usuario
  cree estar manejando arruinan la sensación de tocar.

## Calidad de audio

- **Ruidoso ≠ distorsionado.** Un timbre sucio se hace con síntesis (parciales inarmónicos, ruido
  filtrado, saturación por bandas), no rompiendo la señal. El bit-crush, el diezmado y el drive en
  el bus producen aliasing: frecuencias que no pertenecen a ninguna nota.
- **Dos envolventes por golpe percusivo** — una para el tono y otra para el ruido. Un transitorio
  de ruido corto sobre un cuerpo largo es lo que separa un bombo de un "pfff".
- Decaimientos exponenciales: un golpe suena ~3·tau. Un tau de 0.4 s convierte el bombo en un
  drone que tapa todo lo demás.
- **PolyBLEP** en toda forma de onda con esquinas (sierra, cuadrada, pulso).
- Bloqueador de continua a la salida, y sidechain del bombo sobre el resto si hay percusión.
- **Satura sólo lo que tiene UNA parcial fuerte** (bombo y sub). Ni la caja (dos parciales de
  cuerpo) ni el tom (fundamental + parcial de parche): saturar dos parciales deja un tono puro en
  su diferencia, o sea otro pitido. Saturar un metal o una
  campana FM genera sumas y diferencias entre sus parciales inarmónicos, y esas caen en la banda
  de otro instrumento (medido en `drum_poder`: la campana del ride pasaba a dominar en 250–900 Hz,
  encima del cuerpo de la caja).
- **En FM, el ratio del modulador va siempre > 2.** Con menos, la primera lateral inferior queda
  en `|1-ratio|·f`, o sea *debajo* de la fundamental, y se mete en los toms.
- **Un parche por instrumento**: bombo, caja, cada tom, hi-hat y sub monofónicos, con fast-kill de
  3 ms al retriggear. Es lo real (el choke del hi-hat es esto) y evita que un doble pedal apile
  catorce bombos.
- **En una drum machine la percusión NO hace melodía, ninguna.** La batería puede estar AFINADA
  con las notas MIDI (el sub en la tónica de la tonalidad), pero NADA sigue el acorde — ni el sub.
  El bombo, los toms y el cuerpo de la caja son percusión afinada con su altura fija, y todo lo
  que vive arriba de 1 kHz es ruido filtrado, que no tiene nota y por eso no puede hacer melodía.
  El decay va con presupuesto por banda (abajo de 100 Hz hasta 0.70 s de tau; 100-250 hasta 0.22;
  250-1k hasta 0.10; arriba de 1 kHz hasta 0.14). La melodía es cosa del MIDI.
- **El transitorio de un golpe se hace con RUIDO FILTRADO, nunca con un seno.** Un seno corto en
  1-2 kHz dentro de un golpe es un PITIDO pegado al golpe (pasó con el "mazo" del bombo, y costó
  tres versiones encontrarlo). Un mazo, un click o un crack son madera/metal contra un parche: una
  banda de ruido. Regla verificable: ningún golpe de percusión puede tener un bin que sobresalga
  más de 12 dB sobre sus vecinos a un tercio de octava, arriba de 600 Hz.
- **Un bombo sintetizado pesa con TRES bandas**: la fundamental (39-54 Hz) da el peso, el 2º
  armónico con nivel de verdad (80-110 Hz) da la PEGADA EN EL PECHO — es la que se olvida, y sin
  ella el bombo suena flojo y lejano en cualquier parlante chico — y el mazo (0.9-1.9 kHz, 20 ms)
  da la definición. Más saturación propia, que acá sí va porque tiene una sola parcial fuerte.
- **Nada de percusión "de palo" en una máquina de este idioma.** Un ruido pasa-banda corto en
  2.5-4 kHz se oye como cencerro, clave o borde de caja, o sea percusión latina. Si hace falta
  algo arriba que no sean hats, que sea un EFECTO (barrido de ruido de caída o riser, Q bajo) y
  que aparezca en las transiciones, no como pulso.
- **El limitador va con lookahead** (~64 muestras) y el **pasa-bajos del master va DESPUÉS** de él:
  sin lookahead el pico lo termina agarrando el clipper final (distorsión en cada golpe), y un
  limitador modula ganancia, o sea que genera bandas laterales que hay que filtrar después.
- **Una reverb de peines se normaliza por su realimentación** (`x *= k·(1-fb)`): si no, crece al
  alargarla y en un patrón denso florece hasta comerse el limitador.
- **`Serial.setTxTimeoutMs(0)` en todo firmware de audio que imprima.** Un `Serial.print` por USB
  CDC BLOQUEA hasta que el host lea (~100 ms con el monitor cerrado): el loop no rellena el buffer
  del DAC, el DMA se queda sin datos y se oye un chicharreo justo al apretar un botón.
- **Cero robos de voz.** Robar una voz es cortar un golpe en seco, o sea un clic: si la polifonía
  se agota, el problema son las colas largas, no la cantidad de pistas.
- **Los agudos molestos son un bug, no un gusto.** Hats y platos con **pasa-banda**, nunca con
  pasa-altos pelado (deja pasar todo hasta Nyquist y esa octava de arriba es sólo filo); techo
  fijo del master en ~13 kHz; la **resonancia de un filtro atada a su corte** (nunca un pico
  resonante arriba); **toda la saturación antes del filtro del master** (si va después,
  reinyecta agudos que el filtro ya no puede sacar); y la ganancia del **limitador suavizada**
  (~0.5 ms): un escalón de ganancia por muestra es un click de banda ancha.
- **Cuidado con el umbral de apagado de la voz.** A -64 dB, una cola de tau 1.5 s mantiene la voz
  ocupada 11 s después de dejar de oírse y la polifonía se agota tocando normal; -48 dB la libera
  3 veces antes y el corte no se escucha.

## Verificación

No mandes archivos de audio renderizados: la placa está al lado y se escucha ahí. Para validar
lógica de un firmware de audio antes de flashear, se compila el `.ino` en el PC con mocks de
`Arduino.h` / `Wire.h` / `i2s_std.h` y se renderiza a WAV local — eso encuentra NaN, clipping,
notas fuera de escala y voces trabadas que `arduino-cli` no ve.

## Flashear

- **Sin Arduino IDE:** sirve `tools/percu_control/` por HTTP y usa el botón ⚡ FLASH FW
  (ESP Web Tools, Chrome o Edge).
- **Con Arduino IDE:** placa *ESP32S3 Dev Module*, USB CDC On Boot *Enabled*, Flash Mode *DIO*,
  PSRAM *OPI*, monitor a 115200.
