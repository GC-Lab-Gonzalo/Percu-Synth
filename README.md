# PercuSynth

**PercuSynth** es un laboratorio portátil de experimentación con electrónica, programación y síntesis de audio, desarrollado por **GC Lab Chile**.

La percusión y los sintetizadores son la excusa: el arte y la música son la puerta de entrada a un proceso de aprendizaje más amplio, donde explorar electrónica, código y tecnología se vuelve natural, entretenido y con sentido. Es un proyecto diseñado bajo la metodología **STEAM** — donde la ciencia, la tecnología, la ingeniería y las matemáticas se integran con el arte como motor creativo.

<p align="center">
  <img src="Imagenes/percu-synth modelo 3d isometrica.jpeg" alt="PercuSynth - modelo 3D" width="600"/>
</p>

El hardware es compacto, portátil y basado en el microcontrolador **ESP32-S3**. Desde él se puede experimentar con síntesis de sonido, secuenciadores, controladores MIDI, sensores de movimiento, sampleo, luces reactivas y mucho más. Los firmwares y las herramientas web disponibles son solo el punto de partida — **el proyecto está en desarrollo activo** y las posibilidades son abiertas.

---

## ¿Qué se puede hacer con PercuSynth?

El laboratorio permite explorar una amplia variedad de ideas, por ejemplo:

- Sintetizadores y máquinas de ritmo con síntesis de audio en tiempo real
- Controladores MIDI para software de producción musical
- Secuenciadores de pasos con patrones grabables
- Detección de golpes y gestos con sensores piezoeléctricos e IMU
- Instrumentos por impacto y drones por vibración (el equipo apoyado en el piso)
- Reproducción de samples y loops cargados desde el navegador
- Experimentos con filtros, osciladores, envolventes y efectos digitales
- Visuales reactivas en tira o matriz LED WS2812 sincronizadas con sonido o gesto
- Síntesis audiovisual de video y videojuegos controlados por el hardware
- Máquinas generativas que componen solas: canciones, paisajes sonoros y ambientes de película
- Conexión con servicios de IA desde la propia placa: pedir un sonido o una canción hablando
- Integración con software como Ableton, GarageBand, Pure Data, etc.

Los firmwares y herramientas del repositorio son ejemplos concretos de estas posibilidades. El proyecto crece con cada experimento nuevo.

---

## Hardware

<p align="center">
  <img src="Imagenes/percu-synth pinout.jpeg" alt="PercuSynth - Pinout" width="600"/>
</p>

> **Tip para usar con IA:** puedes subir directamente la imagen del pinout a cualquier asistente de IA (Claude, ChatGPT, Gemini, etc.) para que entienda el hardware y te ayude a crear nuevos firmwares. Mejor aún: pásale el [documento de contexto para IA](PROMPT_PARA_LA_IA.md) ([versión PDF](PROMPT_PARA_LA_IA.pdf)) — un documento madre con pinout, settings del Arduino IDE, librerías y patrones de código listos para que la IA genere firmware a la primera.

### Componentes principales

- **Microcontrolador:** ESP32-S3 (USB nativo, WiFi, Bluetooth)
- **DAC de audio:** PCM5102 vía I2S — salida estéreo 44.1 kHz · 16-bit
- **IMU:** MPU6050 — acelerómetro + giroscopio para control gestual (I2C)
- **Entradas:** 5 botones, 4 potenciómetros, 4 sensores piezoeléctricos, 2 sensores analógicos externos
- **Salidas:** Audio I2S · tira/matriz LED WS2812 · MIDI USB nativo · MIDI DIN-5

> **¿Quieres armar el tuyo?** La [lista de materiales](MATERIALES.md) tiene cada componente con su término de búsqueda, los tres caminos para armarlo (PCB fabricada, protoboard o placa perforada) y cómo pedir la placa en JLCPCB. Comprar la PCB es opcional.

### Pinout

| Señal | Pin ESP32-S3 |
|-------|--------------|
| I2S LRCK | 39 |
| I2S DATA | 40 |
| I2S BCLK | 41 |
| Botones | 44, 42, 0, 45, 47 |
| Potenciómetros | ADC 1, 2, 8, 10 |
| Piezos | ADC 4, 5, 6, 7 |
| LED WS2812 (datos) | 46 |
| MIDI DIN-5 TX | 43 |
| I2C SDA (MPU6050) | 21 |
| I2C SCL (MPU6050) | 38 |
| Sensor externo A | ADC 3 |
| Sensor externo B | ADC 9 |

Todos los firmwares de audio generan señal a **44.1 kHz, 16-bit estéreo** a través del DAC PCM5102, con buffers DMA de 128 muestras.

### Archivos de circuito (`Hardware/`)

El diseño electrónico está abierto para que puedas fabricar tu propia placa o estudiar las conexiones:

La versión vigente es la **V2.0** (septiembre 2026): cada entrada de piezo lleva ahora diodo Schottky 1N5817 + 10 nF + 100 kΩ, y la placa trae sitio para el micrófono INMP441 y un conector OLED opcional. La lista de materiales completa está en [`MATERIALES.md`](MATERIALES.md).

- [`Schematic_Percu-synth_V2.0.pdf`](Hardware/Schematic_Percu-synth_V2.0.pdf) — esquemático completo
- [`PCB_1-PCB_PCB_Percu-synth_V2.0.pdf`](Hardware/PCB_1-PCB_PCB_Percu-synth_V2.0.pdf) — vista del PCB (top y bottom) para ubicar componentes
- [`Gerber_Percu-synth_1-PCB_PCB_Percu-synth_V2.0.zip`](Hardware/Gerber_Percu-synth_1-PCB_PCB_Percu-synth_V2.0.zip) — gerbers listos para enviar a fabricar

Los archivos `V1.1` siguen en `Hardware/` sólo como referencia de las placas antiguas.

---

## Firmwares disponibles

Cada firmware es un sketch Arduino independiente (`.ino`). Se compila y se carga por separado — no hay sistema de build centralizado. Los siguientes son los firmwares desarrollados hasta la fecha; el proyecto está en desarrollo activo y se irán sumando más experimentos con el tiempo.

### Síntesis y secuenciadores de audio

#### `drum_machine_basic` — Drum Machine con Secuenciador
- 10 voces polifónicas: kick, snare, hi-hat, crash, click
- Síntesis por osciladores + ruido LCG + filtros biquad bandpass en cascada
- Secuenciador de 4 pistas × 16 pasos con grabación en tiempo real
- Control de tempo y timbre por potenciómetros

#### `drum_ruido` — Drum machine de timbres ruidosos, salida limpia
- **Ruidoso no es distorsionado:** la suciedad se hace con *síntesis* (parciales inarmónicos, dos envolventes por golpe, saturación por bandas), nunca rompiendo el audio — nada de bit-crush ni diezmado, que solo producen aliasing
- **Los patrones son aleatorios:** no hay ritmos de fábrica. Cada BTN1 sortea 32 pasos nuevos con el "feel" del kit activo
- 5 kits (CYBER · DUBSTEP half-time · GLITCH · INDUSTRIAL · CAOS) y 7 pistas: kick, snare, hats, clank, metal, blast y bajo
- Un botón = una función: BTN1 patrón · BTN2 timbre · BTN3 tap tempo · BTN4 half-time · BTN5 fill. POT4 = beat repeat ×2/×4/×8/×16 sobre un reloj maestro que nunca se desfasa
- No hay play/stop: la máquina siempre corre, POT1 a cero es el mute. Requiere **FastLED**

#### `drum_poder` — Drum machine con peso + reloj MIDI por el DIN-5
- Hermana de `drum_ruido` con el signo cambiado: **acá no hay ruido como estética**. Bombos redondos con pegada de mazo, cajas grandes y afinadas, toms cantados, platos con brillo controlado
- **12 grooves escritos a mano** en el idioma de Sleep Token, Tool y Dream Theater, con compases impares de verdad: 7/8 (3+2+2), 5/8+7/8, 9/8, 13/8, 5/4, 12/8, doble bombo y 4/4 desplazado (3+3+3+3+2+2). Los patrones se escriben **como texto**, un carácter por semicorchea
- **Por el MIDI DIN-5 sale el RELOJ, no notas** (GPIO 43, 31250 baud): MIDI Clock a 24 PPQN + Start, y Stop+Start cada vez que se reubica el "1". La melodía la pone el sinte que lo recibe. Hubo tres versiones con bajo adentro (línea escrita por groove, tabla de ritmos sobre una grilla, patrones derivados del bombo) y las tres se descartaron: elegir el sonido y la secuencia en el sinte es más expresivo que cualquier tabla metida en el firmware, y el único trabajo real de la máquina es dar un pulso que no se mueva. Verificado: el tempo del reloj cae dentro del **0.04 %** del BPM en los 12 grooves
- **Es percusión y nada más**: la melodía la hacen las notas MIDI. Sólo el SUB (abajo de 60 Hz) sigue el acorde; todo lo que vive arriba de 1 kHz es **ruido filtrado** — no tiene nota, así que no puede hacer melodía —, y el decay tiene presupuesto por banda: el peso abajo es largo (sub 2.1 s, bombo 1.3 s) y arriba todo es corto (hat 0.13 s, tick 0.07 s)
- 8 pistas con **su propia banda** cada una (sub, bombo, toms, caja, clap, tick, hats, metal) y cadena de master fija: sidechain → sala corta → saturación de cuerpo → filtro → compresor de bus → limitador **con lookahead** → techo de 13 kHz
- **Exactamente los controles de `drum_ruido`, uno por uno**: BTN1 groove · BTN2 kit · BTN3 tap tempo · BTN4 medio tiempo · BTN5 redoble; POT1 volumen · POT2 filtro · POT3 cuerpo · POT4 beat repeat. Sin paneles, sin combos y sin pots congelados: todo eso existió mientras el firmware generaba las notas del bajo
- **El SUB lleva 2º y 3er armónico**: su fundamental vive en 33–45 Hz, así que en un parlante chico lo que se oye son los armónicos y el oído reconstruye la que falta
- **El audio corre en su propia tarea en el core 1** y los controles en el core 0. El ADC y `FastLED.show()` compartiendo `loop()` con el render eran lo que hacía que el DMA se quedara sin datos en los patrones densos y en los fills: eso era el chasquido. Y por lo mismo **no hay ni un `Serial.print`**. Requiere **FastLED**

#### `synth_basico` — Sintetizador Polifónico
- 5 voces con morphing de forma de onda (senoidal → cuadrada → diente de sierra)
- Vibrato LFO (±1.2%, 0.2–8.2 Hz) y filtro pasa-bajos one-pole controlados por potenciómetro
- Notas mapeadas a los botones: C4, D4, E4, F4, G4

#### `trance_imu` — Secuenciador de Trance Polifónico (IMU)
- Port del secuenciador de trance del **Proto-Synth v2** a I2S 44.1 kHz / 16-bit estéreo (audio de otra liga, buffers DMA no bloqueantes)
- **Polifónico:** cada paso dispara un acorde de 4 voces sobre un pool de 16 voces → textura "pluck de trance" con cola
- Osciladores sierra anti-aliasing (PolyBLEP) + filtro pasa-bajos resonante biquad **controlado en vivo por el IMU** (eje X → cutoff, eje Y → resonancia)
- 3 paneles de control (combos de botones): normal / notas-tonalidad / timbre-síntesis. Sin LEDs ni Serial: todo el CPU va al audio

#### `trance_imu_leds` — Trance Polifónico + 6 LEDs de placa
- Igual que `trance_imu`, pero usa los **6 LEDs WS2812 SMD internos** de la placa como visualizador (VU polifónico + flash al beat + color según el filtro IMU + paleta por panel)

#### `pads_imu` — Pads ambient profundos (sin secuenciador)
- Hermano ambient de `trance_imu`: **mismo motor de audio, pero sin patrones**. Los 5 botones latchean **acordes sostenidos** (attack → sustain → release sobre un pool de 32 voces, estéreo con detune y paneo por voz)
- **5 bancos de 6 acordes** + capa opcional de **arpegio** (6 tipos) sobre el acorde activo
- **Modo AUTO:** progresión diatónica generativa que se repite en 4/4, para dejarlo sonando solo
- 3 paneles de control; el IMU barre el filtro

#### `pads_imu_leds` — pads_imu + 6 LEDs de placa
- Igual que `pads_imu`, con los 6 LEDs SMD como visualizador: paleta por panel, barra de energía del pad y un **punto que avanza con cada nota del arpegio**. También maneja el LED RGB del módulo (GPIO48). Requiere **FastLED**

#### `cancion_aleatoria_leds` — Máquina de canciones aleatorias
- **Autónoma:** un botón de Play/Stop y tres perillas de volumen (pad / melodía / percusión). Nada más
- Cada Play genera **una canción entera y coherente**: sortea tonalidad, modo (7 escalas), progresión diatónica, BPM, voicing, síntesis y ritmo del pad
- Encima corre una **melodía monofónica generativa** organizada en frases (cada frase sortea su propio ritmo, timbre, registro y envolvente) que siempre cae en la escala
- Y una **percusión sintetizada aleatoria** (kick, snare-rim, hats — sin samples) con acentos, ghost notes y un mini-fill cada 4 compases
- 5 arquetipos (AMBIENT / CINEMATIC / PULSE / PLUCK / DRIVE) fijan el carácter global, así que las canciones suenan **categóricamente distintas** entre sí. Stop → Play = canción nueva. Requiere **FastLED**

#### `paisajes_relax_leds` — Paisajes sonoros para relajar
- **10 capas sintetizadas en tiempo real** (sin samples) que se combinan libremente: viento, olas, campanitas, gotas de agua, grillos, fogata, lluvia, cuenco tibetano, arroyo y pájaros
- Cada botón enciende una capa (toque = principal, mantener >0.6 s = alterna). Todo **seco y natural**, sin delay ni reverb
- Dos anillos WS2812 de 30 LEDs (afuera = eventos, adentro = ambiente). Arranca en silencio. Requiere **FastLED**

#### `cyber_kit` — Secuenciador de texturas, FX y leads
- **No es una drum machine:** 4 bancos × 5 sonidos (LEADS / TEXTURAS / FX / BAJOS) — leads neón, campanas FM, hoovers, risers, nubes granulares, drones oscuros, zaps, glitches, reeses, subs
- Los sonidos afinados viven **dentro de una escala seleccionable** (10 escalas, tónica variable)
- **Disparo instantáneo:** los botones suenan en el flanco de presión, y los combos de dos botones *deshacen* la acción individual con un fade de 4 ms. Se toca al tiro y los combos igual funcionan
- 2 modos: PERC (los botones disparan) y SEQ con transporte completo (play/stop, reversa, beat repeat, velocidad, caos)
- 4 paneles de pots, incluido uno de **macros de síntesis** (attack 0.5 ms–0.8 s, decay, textura, pitch global ±12). Requiere **FastLED**

#### `oscilador_escalas` — 4 pots = 4 osciladores por escala
- Port del sketch `Oscilador_4_escalas` del **Proto-Synth v2**, **sin Mozzi**, al motor I2S estéreo de 44.1 kHz del PercuSynth
- La idea original intacta — cada pot es un oscilador cuantizado a la escala activa — pero por debajo: stack de **3 voces en unísono** paneadas + sub-oscilador, formas de onda **PolyBLEP**, portamento de 40 ms y cuantización **con histéresis** (el ruido del ADC ya no hace saltar la nota)
- Controles directos, sin paneles ni combos: BTN1 escala (10) · BTN2 octava · BTN3 intermitencia · BTN4 tap tempo · BTN5 forma de onda
- Filtro resonante barrido por el eje X del IMU + **delay ping-pong** cuyo tiempo lo fija el tap tempo. Requiere **FastLED**
- Audio en su propia tarea en el **core 1** y controles en el core 0: así los 4 osciladores en cuadrada o pulso ya no vacían el DMA (era el ruido que aparecía al cambiar de onda)

#### `oscilador_escalas_clock` — El mismo dron + reloj MIDI por el DIN-5
- `oscilador_escalas` con **MIDI Clock por el DIN-5** (24 PPQN, sólo reloj, nada de notas) al tempo del tap: **Start al activar la intermitencia, Stop al apagarla, Stop+Start en cada tap** para que el "1" del sinte externo caiga en el mismo corte
- El clock se **cuenta en el audio** con el mismo contador que el corte (cero deriva) y se **manda desde la tarea de control** a 1 kHz. Los ticks salen siempre, aun parado, para que el sinte ya tenga el tempo cuando llegue el Start
- Los **6 LEDs prenden al ritmo de la intermitencia** (siguen la envolvente que corta el audio) y el **LED RGB del módulo muestra el color de la nota** que suena, pasando al siguiente oscilador activo en cada pulso. Requiere **FastLED**

#### `espacio_modular` — Ambientes de película (monofónico)
- **24 patrones que son TEMAS, no figuras:** un tema se define tanto por su ritmo largo-corto como por sus notas, y **cada nota dura hasta la siguiente** — el espaciado *es* la duración. Por eso suena a música de película y no a un arpegio
- Debajo, un **dron continuo** en la tónica y una **progresión no funcional** de un acorde por ciclo de 4 compases (9–38 s por acorde): eso es lo que lo vuelve ambiente en vez de canción
- 8 modos de paleta cinematográfica (eólico, frigio, NÓRDICO, mixolidio, lidio, ÁRABE, BIZANTINO, menor armónica), todos con tríadas consonantes
- Un solo panel: 5 botones y 4 pots, una función cada uno, sin combos ni páginas escondidas. Play sortea el modo, BTN2 sortea el tema
- Imprime modo/tema/tonalidad por Serial en cada botón, para que puedas identificar un sonido que te gustó

#### `impact_chimes` — Campanas por golpe en el piso
- Instrumento por **impacto**: se apoya el equipo en el piso y el **acelerómetro detecta los golpes** → dispara notas de una escala tipo campanas (eólica por defecto)
- Una escala distinta por cada botón; los potenciómetros controlan la síntesis

#### `impact_chimes_leds` — Campanas por impacto + show de luces
- Hermano de `impact_chimes` con **show WS2812 de 68 LEDs** y **3 timbres** (campana / marimba / guitarra eléctrica), en una sola escala mágica: **C lidio**
- Cada golpe en el piso dispara una nota (caminata melódica) y un efecto de luz reactivo. BTN1/BTN5 recorren 5 efectos (onda, cometa, pulso, chispas, arcoíris); BTN2/3/4 eligen el timbre
- Umbral dinámico anti-doble-disparo; FastLED por RMT para no chocar con el I2S. Requiere **FastLED**

#### `laser_chimes` — Campanas al cortar el haz de un láser
- Hermano de `impact_chimes` con el **sensor de entrada cambiado**: un **LDR + 220 Ω en el sensor externo A (EXT1, GPIO 3)** vigila un **haz de láser**; cada vez que algo lo cruza se dispara una nota de la escala
- Mismo motor de sonido y mismos controles que `impact_chimes` (una escala por botón, pots = ataque/decay/brillo/timbre). El corte más profundo suena más fuerte
- El LDR se muestrea a **1 kHz en el core 0** (el audio se queda con el core 1) y la cola de DMA es corta (≈12 ms): la nota se oye en el acto
- Los **límites de lectura del LDR** (`LDR_LASER` / `LDR_TAPADO`) son variables en el `.ino`; con `MOSTRAR_ESTADO 1` el Monitor Serie imprime `raw` y los `min`/`max` vistos para copiarlos directo. Sin librerías externas

#### `seismic_drone` — Drones épicos por vibración de la tierra
- Hermano grave de `impact_chimes`: el MPU6050 en **±2g** detecta la vibración del suelo → genera un **dron épico** (sierra estéreo desafinada + sub-oscilador, filtro resonante que "respira")
- Escalas épicas; los potenciómetros controlan la textura

#### `dub_siren` — *(en desarrollo)*
- Sintetizador "sirena dub" con 3 samples polifónicos + oscilador siren con LFO + delay tape con feedback
- Generación del firmware (con samples embebidos) vía `tools/dub_siren_generator/`
- Plan completo y arquitectura en [`firmwares/dub_siren/PLAN.md`](firmwares/dub_siren/PLAN.md)

### Firmwares con IA (requieren WiFi y claves de API)

Estos seis firmwares llaman a servicios externos (OpenAI, NagaAI, ElevenLabs) desde el propio
ESP32-S3. Necesitan además un **micrófono INMP441** por I2S y, salvo los dos asistentes simples
(`asistente_ia` y `asistente_naga`), un módulo **con PSRAM**.

> **Las claves no están en el repositorio.** Cada uno de estos sketches trae un
> `secretos.example.h`: cópialo a `secretos.h` **en la misma carpeta** y escribe ahí tu WiFi y tus
> API keys. `secretos.h` está en el `.gitignore`, así que nunca se sube por accidente. Si falta,
> el sketch no compila y te avisa con un `#error` claro en vez de fallar recién al encender.

#### `asistente_ia` — Asistente de voz (Whisper → GPT → TTS)
- Mantienes BTN1, hablas, y te responde por el parlante. El más simple de la familia: el que conviene leer primero para entender cómo se conecta el hardware a una API
- Micrófono INMP441 (16 kHz mono) → Whisper → GPT-4o-mini → TTS (PCM 24 kHz) → DAC PCM5102, sin resamplear
- Los 6 LEDs SMD indican el estado: verde listo, rojo grabando, ámbar procesando, cian hablando

#### `asistente_naga` — El mismo asistente, sobre NagaAI (una sola clave, modelos gratis)
- [NagaAI](https://naga.ac/) es un **agregador**: una API compatible con OpenAI que enruta a muchos proveedores con **una sola clave y un solo saldo**. Los endpoints son idénticos; lo que cambia es el host, los **nombres de modelo** y un parámetro
- Ojo con eso último: **`whisper-1`, `tts-1` y `gpt-4o-mini` no existen en Naga** (hay `whisper-large-v3`, `gpt-4o-mini-tts`, `llama-3.3-70b-instruct:free`, voces de ElevenLabs…), y el límite de respuesta es `max_completion_tokens`, no `max_tokens`. Copiar los nombres de OpenAI tal cual da 404 y silencio
- Viene con los tres modelos **`:free`** puestos por defecto: funciona **sin gastar saldo**, que es lo que hace falta en un taller con diez placas
- La reproducción **lee la cabecera WAV** en vez de asumir 24 kHz: frecuencia, canales y bits vienen en el archivo y el I2S se reconfigura en caliente, así cambiar de voz no obliga a tocar ninguna constante. Si llega un MP3 lo detecta y **lo avisa por Serial** en vez de reproducir ruido
- `MOSTRAR_ESTADO` imprime todo el recorrido a 115200: lo que entendió, lo que respondió, los códigos HTTP con el cuerpo del error y el formato de audio recibido

#### `asistente_musical` — Conversas con GPT sobre música que nunca se detiene
- Cruce de la cadena de red de `asistente_ia` con el motor de voces de `pads_imu`: **hablas con GPT mientras un fondo armónico generativo sigue sonando**
- El cambio de arquitectura que lo hace posible: un **único mixer de 44.1 kHz** dueño del I2S corriendo en el **core 1**, y el asistente en el **core 0** junto al stack de WiFi — así los segundos que bloquea el TLS no cortan la música
- El TTS llega a 24 kHz y se **resamplea a 44.1 kHz**, pasa por un pasa-altos con cutoff en un pot (limpio → megáfono) y se suma **después** del filtro del pad
- **Ducking sidechain** real: la voz baja la música. Y BTN1 apaga el pad mientras grabas, porque el micrófono escucha al parlante y Whisper transcribiría la música

#### `sampler_ia` — Pides un sample hablando y lo disparas
- Dices el sonido que quieres → Whisper transcribe → GPT lo convierte en un prompt de efecto en inglés → **ElevenLabs** lo genera → se recorta, normaliza y carga en un slot de PSRAM
- 3 slots: BTN2/3/4 disparan en el flanco de presión; **manteniendo >0.6 s** ese slot pasa a loop sostenido. BTN5 corre un secuenciador de 16 pasos donde puedes **hacer overdub** tocando en vivo
- Los slots vacíos **prestan el último sample transpuesto** (+3 y +7 semitonos = una tríada menor de un solo sonido)
- Pots: BPM, volumen, pitch en vivo (±12) y **stutter granular**; el IMU barre el filtro. Tira de 80 LEDs encadenada a los 6 internos
- El formato `pcm_22050` requiere plan Pro de ElevenLabs: en Starter **cae solo a `ulaw_8000`**, decodificado en línea

#### `oscilador_ia` — El sample de IA *es* el oscilador
- Pides un **sonido** por voz y ese sample se vuelve el oscilador del instrumento. A ElevenLabs se le manda solo el sonido traducido al inglés: **no entiende de notas ni de Hz**, y las coletillas musicales lo confunden
- Al cargar, el firmware **mide la fundamental real por autocorrelación** (con guarda de error de octava y refinamiento parabólico), así que queda afinado al centésimo *donde sea* que haya caído el sample
- Un **loop de sustain con crossfade horneado** deja que las notas suenen para siempre: drones infinitos con el sample como oscilador continuo
- 10 escalas con vocabulario de progresiones por modo. 6 modos: teclado, 4 arpegiadores y una secuencia generativa de 4 compases. Cambiar la escala reinterpreta los mismos grados = recoloreado modal instantáneo

#### `compositor_ia` — Le pides una canción con la voz
- Fusión de `asistente_ia` (mic → Whisper → GPT) con el motor de `cancion_aleatoria_leds`: **GPT responde un JSON de canción** y el PercuSynth la toca
- Bajo con 5 patrones, comping por golpes, 5 gramáticas de melodía, forma con secciones (intro → verso → coro) y **swing real** en todo el grid
- El prompt completo vive en `COMPOSER_PROMPT` dentro del sketch: puedes pedirle el JSON a cualquier IA en el PC y **pegarlo por el Monitor Serie, sin WiFi**

### Controladores MIDI y máquinas audiovisuales

#### `MIDI_Drum` — Controlador MIDI de Percusión
- Convierte golpes físicos y gestos en mensajes MIDI USB (canal 10 / GM drums)
- Tres modalidades de entrada: botones (cola circular), sensores piezoeléctricos (peak-detection 15 ms) e IMU (ventana 20 ms)
- Debounce de 50 ms por piezo y 25 ms por botón
- Compatible con cualquier software o hardware que reciba MIDI

#### `drum_midi_leds` — Drum Machine + MIDI + Luces
- Secuenciador de 16 pasos × 4 drums (kick, snare, hi-hat, crash) sincronizado con efectos full-strip cinematográficos en la tira WS2812
- Salida MIDI USB en canal 10 (notas 36, 38, 42, 49) — el PercuSynth queda como controlador de un DAW
- BTN5 alterna entre modo **GRAB** (grabación en tiempo real) y **PLAYBACK**
- Potes para brillo, tempo (60–240 BPM), color base del fondo y velocity MIDI (60–127)
- Cola de NoteOff diferidos para mantener los gates MIDI limpios

#### `trance_midi_leds` — Trance melódico MIDI + matriz 20×20
- El mismo motor de secuenciador de `trance_imu` pero **monofónico y melódico**: cada paso envía **una sola nota** por MIDI USB a tu DAW/sinte (true mono, sin notas solapadas)
- El IMU se traduce a **MIDI CC** (CC74 filtro / CC71 resonancia) para barrer el filtro del sinte moviendo el aparato
- En paralelo, la matriz 20×20 corre un show estilo **fiesta electrónica** reactivo al beat

#### `matrix_midi_anyma` — Máquina audiovisual electro (matriz 20×20)
- Máquina **estilo Anyma** que combina tres cosas: **secuenciador interno** de 16 pasos (drums ch10 + bajo *acid* ch1 por USB MIDI), **MIDI Clock Master** (24 PPQ) y un **motor visual 2D** de 5 escenas sobre la matriz WS2812 20×20
- Las visuales reaccionan tanto al secuenciador interno como a **notas MIDI entrantes** (un secuenciador externo también pinta la matriz)
- No genera audio: es controlador MIDI + visualizador

### Pruebas de hardware

Sketches mínimos de diagnóstico para verificar cada periférico. Varios evitan el USB/Serial a propósito (el CDC puede provocar reinicios durante las pruebas):

- **`test_system`** — **Empieza por aquí con una placa recién armada.** Vuelca por el Monitor Serie el estado en vivo de *todos* los componentes en una sola pantalla, y acepta comandos para lanzar auto-tests de audio y de luces
- **`test_leds`** — Test de la tira LED WS2812: 6 modos de animación (sólido, chase, rainbow, twinkle, pulso, meteor). LEDs 0-5 (SMD internos) siempre apagados; del 6 en adelante activos. Requiere **FastLED**
- **`test_imu`** — Comprobación mínima del IMU por **Monitor Serie**: WHO_AM_I, aceleración (g) y giro (°/s) de los 3 ejes
- **`test_imu_led`** — Comprueba el IMU **sin USB ni Serial**; el resultado se ve en la **tira LED**
- **`test_imu_sound`** — Comprueba el IMU por **sonido** (DAC), sin LEDs, USB ni Serial

---

## Herramientas web (`tools/`)

Páginas HTML standalone (sin build, sin npm) que cumplen distintos roles según la herramienta:

1. **Generadores de firmware** — drag & drop de audios → la webapp produce un `.ino` con los samples embebidos en flash como arrays `PROGMEM`. Lo flasheas con Arduino IDE y la web no se usa más hasta que cambies los samples. (`sample_loader`, `loop_loader`, `dub_siren_generator`, `loops/bpm_mono_44100`)
2. **Flasheo desde el navegador** — instala el firmware compilado directo al ESP32-S3 vía **ESP Web Tools**, sin abrir Arduino IDE. (`percu_control`)
3. **Control remoto en vivo** — se conecta al PercuSynth ya flasheado vía **Web MIDI** y edita patrón / FX / transport en tiempo real. (`step_sequencer_loader`)
4. **Instrumento en el navegador** — lee un controlador MIDI USB (**Web MIDI**) y suena al instante con **Web Audio**, además de generar su `.ino`. (`midi_sampler`)
5. **Síntesis audiovisual** — usa la PercuSynth (vía **Web Serial**) como controlador de visuales/sonido en el navegador. (`video_synth`)
6. **Instrumentos y secuenciadores en el navegador** — el PercuSynth como controlador de un motor que suena en la página. (`arp_matrix`, `scale_osc`, `generador_estilos`)

La mayoría requiere **Chrome o Edge** (Firefox/Safari no soportan Web Serial ni Web MIDI). Cada herramienta tiene su propio `README.md` con detalles:

### [`percu_control/`](tools/percu_control/) — Panel de control universal + flasheo
Interfaz visual completa para configurar el PercuSynth: osciladores con 5 formas de onda, octave shift, mixer, ruido (white/pink), drive multimodo (off/soft/fold/bit), filtro, LFO y master. **Incluye un botón "⚡ FLASH FW" que instala el firmware directo al ESP32-S3 desde el navegador** usando ESP Web Tools — no requiere Arduino IDE para usuarios finales.

### [`sample_loader/`](tools/sample_loader/) — Cargador de samples one-shot
Drag & drop de hasta 5 archivos de audio (máx 2 s c/u). **Genera un `.ino` con los samples embebidos en flash como arrays `PROGMEM`**. El firmware generado convierte al PercuSynth en un sampler polifónico con pitch shift cuantizado a escala frigia.

### [`loop_loader/`](tools/loop_loader/) — Cargador de loops + hits con preview
Genera firmware con 3 loops (hasta 8 s, wrap sin click) + 6 hits one-shot (hasta 3 s, polifonía 3 voces). Preview en navegador con sliders que simulan los pots y el IMU.

### [`step_sequencer_loader/`](tools/step_sequencer_loader/) — Secuenciador remoto en vivo
Doble personalidad: **(1)** genera firmware con 6 samples embebidos y secuenciador 6×16; **(2)** una vez flasheado, controla el PercuSynth en vivo vía **Web MIDI** — editar patrón, FX y transport sin re-flashear. El PercuSynth queda como MIDI Clock Master.

### [`dub_siren_generator/`](tools/dub_siren_generator/) — Generador de dub siren
Genera firmware con 3 samples polifónicos + oscilador siren con LFO modulado por POT + delay tape con feedback (hasta auto-oscilación) + pitch global por IMU. Sample rate seleccionable (44.1 kHz / 22 kHz lo-fi).

### [`midi_sampler/`](tools/midi_sampler/) — Sampler MIDI USB (1 sample · 4 pots)
Lee un controlador MIDI USB vía **Web MIDI** y reproduce **un solo sample** (ej. una campana) según la nota MIDI que entra, transpuesto desde una nota base configurable. 4 potenciómetros: volumen/attack/decay/cutoff (también por CC MIDI). Suena en el navegador con **Web Audio** (testeable sin hardware: teclado en pantalla + teclas del PC) y, sin sample cargado, usa un seno afinado. Pestaña aparte para **generar el `.ino`** que convierte al PercuSynth en un sampler MIDI físico de 1 sample.

### [`video_synth/`](tools/video_synth/) — Sintetizador audiovisual de video
Webapp de una página que **importa un video y lo sintetiza en imagen Y sonido** en tiempo real, controlado por la PercuSynth vía **Web Serial** (o el micrófono del PC). Minimalista: cada control hace una sola cosa obvia.

### [`arp_matrix/`](tools/arp_matrix/) — Arpegiador polifónico + matriz 64×32
Proyecto colectivo nacido en el taller del PercuSynth. Arpegiador polifónico con una **matriz LED horizontal de 64×32** reactiva al sonido: 8 modos griegos, 6 patrones, ADSR y 3 paneles de control. Suena en el navegador (**Web Audio**), se controla con el hardware (**Web Serial**) y también **genera el `.ino`**.

### [`scale_osc/`](tools/scale_osc/) — Motor de tono cuantizado
Cada potenciómetro es un **oscilador cuantizado a una escala**: la versión de navegador del firmware `oscilador_escalas`. Sirve para probar el concepto sin flashear nada, o como instrumento en vivo con el PercuSynth de controlador (**Web Serial** + **Web MIDI**, con el TILT del IMU barriendo el filtro).

### [`generador_estilos/`](tools/generador_estilos/) — Motor de estilos musicales *(en desarrollo)*
Un estilo musical entendido como **datos, no como código**: blues, techno, synthwave y grunge descritos en JSON que el motor toca con Web Audio. El diseño completo está en [`PLAN.md`](tools/generador_estilos/PLAN.md) y el prompt para pedirle canciones a una IA en [`PROMPT_IA.md`](tools/generador_estilos/PROMPT_IA.md).

### [`loops/bpm_mono_44100/`](tools/loops/bpm_mono_44100/) — Editor BPM-aware de loops
Variante avanzada de `loop_loader` con tap-tempo, transporte master con beat dots, snap a compás y mono-switcher (un solo loop activo a la vez) + sampler polifónico paralelo. Para sesiones donde los loops tienen que estar sincronizados en BPM.

---

## Videojuegos (`videogame/`)

Los tres se controlan con el PercuSynth vía **Web Serial** (mismo protocolo y mismo firmware de control) y los tres son **totalmente jugables con mouse y teclado** cuando no hay hardware conectado.

### [`cyber_flight/`](videogame/cyber_flight/) — NEON STRIKE
**Shooter cyberpunk en primera persona** sobre una megaciudad distópica: pilotas un caza y derribas naves enemigas. Los **2 ejes del IMU** apuntan la mira, **BTN5/BTN1** disparan desde cada lado y **BTN2+BTN4** juntos sueltan una bomba. Canvas 2D synthwave con efectos de sonido en Web Audio y sistema de oleadas, puntaje y multiplicador.

### [`nebula_gp/`](videogame/nebula_gp/) — NEBULA GP
**Simulador de carreras FPV de drones:** 3 vueltas contra 4 bots en un circuito neón cerrado con lomas y 16 puertas, bajo un cielo galáctico. Proyección 3D hecha a mano sobre Canvas 2D, sin WebGL. El **IMU** da giro y cabeceo, **BTN5** acelera, **BTN1** frena, **BTN2/BTN4** desplazan de lado y **BTN3** recentra el IMU.

### [`tilt_maze/`](videogame/tilt_maze/) — Tilt Maze
Laberinto de bola que se controla **inclinando el PercuSynth**, como los juguetes de madera pero con 10 niveles curados (todos resolubles), vidas, portales, hoyos y zonas mortales.

---

## Samples (`samples/`)

Esta carpeta es una **biblioteca viva de firmwares generados por las webapps de `tools/`**. Cada subcarpeta tiene un `.ino` que ya viene con samples reales embebidos, listos para abrir en Arduino IDE y flashear como ejemplo de qué se puede armar:

- **`Industrial/` · `industrial_dos/`** — kits industriales (generados con `bpm_mono_44100`)
- **`percusynth_samples/` · `percusynth_samples2/`** — bancos propios (generados con `sample_loader`)
- **`percusynth_loop_player/`** — set dub/dance (generado con `loop_loader`)
- **`sonidos/`** — archivos de audio crudos (WAV) listos para alimentar las webapps

> Estos `.ino` son **artefactos generados**, no escritos a mano. Para hacer tus propios kits, abre la webapp correspondiente, carga tus audios y genera un nuevo `.ino`.

---

## Cómo cargar un firmware

### Opción A — Desde el navegador (sin Arduino IDE)

Sirve `tools/percu_control/` con un mini-servidor HTTP y abre la página en Chrome o Edge:

```bash
cd tools/percu_control
python -m http.server 8000
```

Luego abre <http://localhost:8000>, conecta el PercuSynth por USB y aprieta **⚡ FLASH FW**.

### Opción B — Desde Arduino IDE

1. Abrir el archivo `.ino` en **Arduino IDE**
2. Seleccionar placa: **ESP32S3 Dev Module** (o variante equivalente)
3. Configurar: **USB CDC On Boot: Enabled**, **Flash Mode: DIO** (crítico — OPI rompe I2S), **PSRAM: OPI PSRAM**
4. Compilar y cargar al microcontrolador
5. Monitor serie a **115200 baud** para diagnóstico

### Bibliotecas requeridas

- ESP32 Arduino core ≥ 3.x (incluye `driver/i2s_std.h`)
- `Wire.h` — I2C para el MPU6050 *(incluida en el core; la mayoría de los firmwares con IMU leen el sensor por registros crudos, sin librería extra)*
- `USB.h` / `USBMIDI.h` — MIDI USB *(incluidas en el core; MIDI_Drum, drum_midi_leds, trance_midi_leds, matrix_midi_anyma)*
- **FastLED** — la única librería que hay que instalar a mano. La usa todo firmware con LEDs: `test_leds`, `drum_ruido`, `drum_poder`, `drum_midi_leds`, `trance_imu_leds`, `pads_imu_leds`, `cancion_aleatoria_leds`, `paisajes_relax_leds`, `cyber_kit`, `oscilador_escalas`, `oscilador_escalas_clock`, `impact_chimes_leds`, `trance_midi_leds`, `matrix_midi_anyma` y los seis firmwares con IA
- Biblioteca `MPU6050` *(solo MIDI_Drum)*

Los firmwares con IA piden además **PSRAM** (`sampler_ia`, `oscilador_ia`, `asistente_musical`,
`compositor_ia`) y un **micrófono INMP441** por I2S.

---

## Estructura del repositorio

```
percusynth/
├── firmwares/                      # Sketches Arduino escritos a mano
│   ├── drum_machine_basic/         #   Drum machine con secuenciador de pasos
│   ├── drum_ruido/                 #   Drum machine de timbres ruidosos, salida limpia
│   ├── drum_poder/                 #   Drum machine con peso + reloj MIDI por el DIN-5 (12 grooves, compases impares)
│   ├── synth_basico/               #   Sintetizador polifónico con morphing
│   ├── trance_imu/                 #   Secuenciador de trance polifónico (IMU→filtro)
│   ├── trance_imu_leds/            #   trance_imu + 6 LEDs SMD internos como visualizador
│   ├── pads_imu/                   #   Pads ambient sostenidos, sin secuenciador
│   ├── pads_imu_leds/              #   pads_imu + 6 LEDs SMD como visualizador
│   ├── cancion_aleatoria_leds/     #   Máquina de canciones aleatorias (Play = canción nueva)
│   ├── paisajes_relax_leds/        #   10 capas de paisaje sonoro + 2 anillos de LEDs
│   ├── cyber_kit/                  #   Secuenciador de texturas, FX y leads cyber
│   ├── oscilador_escalas/          #   4 pots = 4 osciladores por escala (port sin Mozzi)
│   ├── oscilador_escalas_clock/    #   oscilador_escalas + MIDI Clock por el DIN-5 + LEDs al ritmo de la intermitencia
│   ├── espacio_modular/            #   Ambientes de película: 24 temas sobre dron continuo
│   ├── impact_chimes/              #   Campanas por golpe en el piso (acelerómetro)
│   ├── impact_chimes_leds/         #   impact_chimes + 68 LEDs y 3 timbres (C lidio)
│   ├── laser_chimes/               #   Campanas al cortar un haz de láser (LDR en EXT1)
│   ├── seismic_drone/              #   Drones graves por vibración de la tierra
│   ├── dub_siren/                  #   Dub siren (en desarrollo · PLAN.md)
│   ├── asistente_ia/               #   Asistente de voz (Whisper → GPT → TTS)
│   ├── asistente_naga/             #   El mismo asistente sobre NagaAI: una sola clave y modelos gratis
│   ├── asistente_musical/          #   Hablas con GPT sobre música que nunca se detiene
│   ├── sampler_ia/                 #   Pides un sample hablando (→ ElevenLabs) y lo disparas
│   ├── oscilador_ia/               #   El sample de IA es el oscilador (afinado por autocorrelación)
│   ├── compositor_ia/              #   Le pides una canción con la voz (GPT → JSON → motor)
│   ├── MIDI_Drum/                  #   Controlador MIDI (piezo + IMU + botones)
│   ├── drum_midi_leds/             #   Drum machine + MIDI + LEDs sincronizadas
│   ├── trance_midi_leds/           #   Trance melódico mono por MIDI + matriz 20×20
│   ├── matrix_midi_anyma/          #   Máquina audiovisual electro + MIDI Clock Master (matriz 20×20)
│   ├── test_system/                #   Monitor de sistema: estado de todo por Serie + auto-tests
│   ├── test_leds/                  #   Test de tira LED WS2812 — 6 modos
│   ├── test_imu/                   #   Test del IMU por Monitor Serie
│   ├── test_imu_led/               #   Test del IMU sin USB → resultado en LEDs
│   └── test_imu_sound/             #   Test del IMU sin USB → resultado por sonido
├── tools/                          # Webapps standalone (Chrome/Edge)
│   ├── percu_control/              #   Panel universal + flasheo desde el navegador
│   ├── sample_loader/              #   Genera .ino con samples one-shot
│   ├── loop_loader/                #   Genera .ino con loops + hits
│   ├── step_sequencer_loader/      #   Genera .ino + control remoto vía Web MIDI
│   ├── dub_siren_generator/        #   Genera .ino dub siren con samples
│   ├── midi_sampler/               #   Sampler MIDI USB de 1 sample + 4 pots (Web Audio + genera .ino)
│   ├── arp_matrix/                 #   Arpegiador polifónico + matriz 64×32 (Web Audio + Serial)
│   ├── scale_osc/                  #   Motor de tono cuantizado a escala en el navegador
│   ├── generador_estilos/          #   Motor de estilos musicales por JSON (en desarrollo)
│   ├── video_synth/                #   Sintetizador audiovisual de video (Web Serial)
│   └── loops/
│       └── bpm_mono_44100/         #   Editor BPM-aware de loops sincronizados
├── videogame/
│   ├── cyber_flight/               # NEON STRIKE — shooter cyberpunk (Web Serial)
│   ├── nebula_gp/                  # NEBULA GP — carreras FPV de drones contra 4 bots
│   └── tilt_maze/                  # Tilt Maze — laberinto de bola por inclinación del IMU
├── samples/                        # Firmwares generados por las webapps (ejemplos vivos)
├── Hardware/                       # Esquemático, PCB y gerbers del circuito
├── Imagenes/                       # Renders 3D y diagrama de pinout
├── Documentos/                     # Informe técnico (PDF)
├── PROMPT_PARA_LA_IA.md            # Documento de contexto para IA (+ versión .pdf)
└── Percu-Synth.mp4                 # Video del proyecto
```

¿Dónde está el detalle de cada uno? La mayoría de las subcarpetas de `firmwares/` y `tools/` traen
su propio `README.md` (o un `PLAN.md`, si todavía es un diseño). Los sketches más directos
—`drum_machine_basic`, `synth_basico`, `MIDI_Drum`, `drum_midi_leds`, `trance_midi_leds` y los
`test_*`— se documentan en el **encabezado del propio `.ino`**: ahí está el hardware, los ajustes
del IDE, las librerías y el funcionamiento.

---

## Usar el repositorio con IA

El PercuSynth está pensado para que puedas inventarle firmwares nuevos apoyándote en un asistente
de IA. Hay tres niveles, de menos a más:

1. **Sube la imagen del pinout** ([`Imagenes/percu-synth pinout.jpeg`](Imagenes/)) a cualquier
   asistente (Claude, ChatGPT, Gemini) para que entienda el hardware.
2. **Pásale [`PROMPT_PARA_LA_IA.md`](PROMPT_PARA_LA_IA.md)** ([PDF](PROMPT_PARA_LA_IA.pdf)) — el
   documento madre con el pinout, los ajustes del Arduino IDE, las librerías, las constantes de
   audio y los patrones de código listos para copiar. Con esto la IA genera firmware que compila
   a la primera mucho más seguido.
3. **Usa el repo como skill de Claude Code.** El repositorio incluye
   [`.claude/skills/percusynth/SKILL.md`](.claude/skills/percusynth/SKILL.md): clona el repo, abre
   Claude Code dentro y la skill se carga sola. Además del pinout, le enseña las convenciones del
   proyecto, las reglas de diseño de controles que salieron de los talleres y el checklist de
   calidad de audio.

```bash
git clone https://github.com/GC-Lab-Gonzalo/Percu-Synth.git
```

---

## Proyecto hermano

Este laboratorio es parte del mismo ecosistema que el **[Proto-Synth v2](https://github.com/GC-Lab-Gonzalo/proto-synth-v2)**, otra plataforma de experimentación de GC Lab Chile, con hardware diferente y una colección más amplia de firmwares de ejemplo.

---

Desarrollado por **GC Lab Chile** — electrónica, arte y tecnología abierta.
