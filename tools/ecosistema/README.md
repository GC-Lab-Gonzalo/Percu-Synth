# Ecosistema del PercuSynth

Tutoriales de un vistazo para usar el PercuSynth, con letra grande y pocas palabras. Abre con una sola
pregunta, **¿Qué quieres hacer?**, y ocho respuestas:

| Flujo | Qué recorre |
|---|---|
| **Cargar un firmware** | bajar el repo → preparar Arduino IDE → abrir el .ino → cargarlo → audífonos y a tocar |
| **Juegos** | abrir el juego → bajar su firmware → abrir el .ino → cargarlo con USB CDC On Boot: Enabled → cable a la derecha → Conectar → jugar |
| **Video synth** | abrir `tools/video_synth` → bajar su firmware → cargarlo con USB CDC On Boot: Enabled → cable a la derecha → abrir un video → Conectar → tocar |
| **Resonancia** | abrir gclabchile.com/resonancia → cable a la derecha → instalar su firmware desde la página → Conectar → BTN1 |
| **Controlador MIDI** | abrir el .ino MIDI → cargarlo con USB Mode: USB-OTG (TinyUSB) → cable a la derecha → activar TinyUSB MIDI en el DAW → tocar |
| **Tools: tus sonidos** | abrir `sample_loader` → arrastrar audios → generar el firmware → cargarlo → tocar |
| **Asistentes de IA** | abrir `asistente_ia` → crear y llenar `secretos.h` → cargarlo → luz verde → mantener BTN1 y hablar |
| **Crear un firmware** | contexto a la IA → pedir qué hace cada botón → guardar el .ino → cargarlo → probarlo → Pull Request |

Cada paso es una imagen y una frase: la foto real de la placa (`img/placa.jpg`, de `Imagenes/v2.jpg`)
con el cable en el conector que toca, recortes de una captura real del menú Tools de Arduino IDE
(`img/herramientas.png`, `img/usb_cdc.png`, `img/usb_mode.png`) o de las páginas (`img/video_synth.jpg`,
`img/resonancia*.jpg`) con lo que importa marcado, o un dibujo simple. Cargar el código se
muestra completo en cada flujo: cable a la izquierda, placa y puerto, Flash Mode DIO 80MHz (más USB CDC
On Boot en los juegos y video_synth, y USB Mode TinyUSB en los MIDI) y Subir. Con seis pasos o más, el flujo va en dos filas.
Resonancia es la excepción: instala su firmware desde la página, con el cable en el mismo conector
con que después se conecta (así lo dice la página).

Está sobre la misma mesa de nogal del simulador y del armado.

## Cómo se abre

Doble clic en `index.html` (funciona desde el disco, sin servidor) o:

```bash
python -m http.server 8799 --directory tools/ecosistema
```

Cada flujo tiene su enlace (`index.html#jugar`, `#midi`, `#tools`, `#ia`…) para mandar a alguien directo ahí.

## Los dos conectores

Por el conector **izquierdo** del módulo ESP32 se carga el código; por el **derecho** van el Serial
(juegos, monitor serie) y el USB-MIDI. Los textos salen de la constante `CONECTOR`.

## Cómo está hecho

Un solo `index.html` más las dos imágenes. El contenido está en `FLUJOS` (cada paso es
`[imagen, qué hacer, un detalle]`; `CARGA` y `SUBIR` son los pasos de carga que comparten todos) y las
imágenes en `DIBUJO`: las de la placa recortan la foto con las coordenadas de `FOTO`, las del menú
recortan las capturas con `captura()`. Si cambian los ajustes (otra placa, otra versión del IDE), se
reemplaza `img/herramientas.png` por una captura nueva del menú abierto y se corrigen las filas marcadas.

Regla para lo que se agregue: un paso es una acción en pocas palabras y un detalle de una línea.
