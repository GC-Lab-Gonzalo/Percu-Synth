# sampler_ia — pide un sample con tu voz y dispáralo

Le hablas al PercuSynth (*"un golpe metálico oxidado con cola larga"*), y él lo genera, lo carga
en un slot y lo deja disparable con un botón. Es `sample_loader` **sin PC**: los samples no vienen
precompilados en PROGMEM, se crean en vivo y viven en PSRAM.

```
BTN1 mantener → mic INMP441 → Whisper → GPT (prompt SFX en inglés)
              → ElevenLabs /v1/sound-generation → recorte + normalizado → SLOT
BTN2/3/4 → disparo instantáneo    BTN5 → secuencia
```

## Controles

### Botones

| Botón | GPIO | Toque | Mantener |
|---|---|---|---|
| **BTN1** | 44 | — | **Graba tu pedido de voz** (máx 5 s) y genera el sample. Exclusivo de grabación |
| **BTN2** | 42 | Dispara slot 1 | **> 0.6 s** = LOOP sostenido (textura). Un toque nuevo lo apaga |
| **BTN3** | 0 | Dispara slot 2 | idem |
| **BTN4** | 45 | Dispara slot 3 | idem |
| **BTN5** | 47 | **Play / Stop** de la secuencia (manda Start/Stop MIDI al DAW) | **> 1 s** = borra el patrón |

### Potenciómetros

| Pot | ADC | Rango | Qué hace |
|---|---|---|---|
| **POT1** | 1 | 60 – 200 BPM | Velocidad de la secuencia |
| **POT2** | 2 | 0 – 100 % | Volumen master (curva cuadrática) |
| **POT3** | 8 | ×0.5 – ×2.0 | **Pitch**, −12 a +12 semitonos con detente en el centro. Actúa **en vivo** sobre lo que ya suena, incluidas las texturas en loop |
| **POT4** | 10 | 500 ms → 2 ms | **Stutter granular** (ver abajo) |

### POT4 — el stutter granular

Congela un trocito de la mezcla y lo repite. El largo del grano se acorta a lo largo del recorrido:

| Posición | Grano | Qué se oye |
|---|---|---|
| 0 – 3 % | — | Apagado (con rampa de entrada, no salta) |
| ~1/3 | ~100 ms | Tartamudeo rítmico, tipo beat-repeat |
| ~2/3 | ~15 ms | Granos cortos, textura granulada |
| 100 % | ~2 ms | El grano se vuelve **tono** (~500 Hz): zumbido afinado |

Tres decisiones que lo hacen usable:

- **El grano se acorta exponencialmente**, no lineal. En lineal toda la parte interesante se
  apelotona en el último cuarto del recorrido y el pot se siente muerto.
- **Se recaptura cada ~200 ms** pase lo que pase. Sin eso, un grano corto se queda pegado como
  tono fijo y el efecto deja de seguir a la música.
- **Ventana en los bordes del grano.** Repetir un trozo cortado en seco clickea en cada vuelta, y
  a granos chicos eso son cientos de clicks por segundo.

Va **antes del filtro**, así el pasa-bajos del IMU puede domar el brillo de los granos cortos.

El sample suena siempre completo (el recorte de cola que estaba en este pot se quitó).

### Mixer (mantener BTN2/3/4)

Mientras **mantienes apretado** cualquiera de BTN2/3/4 — el mismo gesto que engancha el loop —
los tres pots cambian de función y se vuelven los faders de canal:

| Pot | En mixer |
|---|---|
| POT2 | Volumen canal 1 |
| POT3 | Volumen canal 2 |
| POT4 | Volumen canal 3 |

POT1 (BPM) no cambia nunca.

**Pots congelados.** Al cambiar de panel, cada pot deja de mandar hasta que lo mueves más de un
umbral, y cada modo recuerda su propio valor. Sin esto, al soltar el botón el volumen master
pegaría un salto a donde quedó el pot. Es el mismo patrón de `pads_imu` / `cyber_kit`.

Los LEDs 0–2 se vuelven VU de los tres faders mientras estás en el mixer: **blanco tenue** = ese
pot sigue congelado, **verde proporcional** = ya tomó el control. El LED 5 se pone violeta.

El fader va por **canal (el botón), no por slot**: si el slot 2 está tocando prestado el sample
del 1, su volumen sigue siendo el del canal 2.

### IMU (MPU6050)

| Eje | Qué hace | Rango |
|---|---|---|
| **X** (inclinar adelante/atrás) | Cutoff del filtro pasa-bajos | ~200 Hz – 12 kHz (curva exponencial) |
| **Y** (inclinar izq/der) | Resonancia (Q) | 0.7 – 7.7 |

Si el IMU no responde, el filtro queda abierto fijo y todo lo demás sigue funcionando. Al arrancar
lo dice por Serial: `IMU listo` o `Sin IMU: filtro abierto fijo`.

## Estéreo

La salida es **estéreo real**. Los samples de ElevenLabs son mono, así que el ancho se
**sintetiza**: cada **canal del mixer** (el botón que aprietas) tiene su lugar fijo en la imagen,
con **pan de potencia constante** — con pan lineal, mover algo del centro a un lado le sube el
volumen.

| Canal | Posición |
|---|---|
| Slot 1 (BTN2) | Izquierda |
| Slot 2 (BTN3) | Centro |
| Slot 3 (BTN4) | Derecha |

Encima, **las texturas latcheadas en LOOP leen el canal derecho 6 ms atrasado** (Haas). En un
sonido sostenido eso abre muchísimo la imagen. En los golpes secos **no se aplica**: ahí el
retardo se oye como un flam y ensucia el transitorio.

Toda la cadena posterior es estéreo: **dos biquad** (el IMU mueve los dos con los mismos
coeficientes — un solo juego de estados mezclaba los canales dentro del filtro) y el **anillo del
stutter intercalado L,R**, para que al entrar el efecto la imagen no se colapse al centro.

> Si el ESP32 se queda corto de CPU, la primera palanca es poner **`HAAS_MS` en 0**: es lo que
> duplica las lecturas de PSRAM en los loops. El pan sigue funcionando igual.

## Reloj MIDI por USB — grabar en Ableton

El PercuSynth se presenta al PC como **dispositivo MIDI** (compuesto con el puerto serie de log) y
**manda reloj**: 24 PPQ (`0xF8`) más **Start** (`0xFA`) / **Stop** (`0xFC`) en BTN5. El tempo es el
de POT1. Es **solo salida**: el PercuSynth manda, el DAW sigue.

El reloj **corre siempre**, también con la secuencia parada, para que el DAW tenga tempo desde que
lo enchufas. Y el secuenciador interno **cuelga del mismo contador de ticks** (una semicorchea = 6
ticks): si cada uno contara por su cuenta, terminarían desfasados aunque partieran del mismo BPM.

**En Ableton:** Preferencias → *Link/Tempo/MIDI*, en la entrada del ESP32 activa **Sync**. Después
pon Ableton en **EXT**.

Tres cosas que conviene saber antes de armar la sesión:

- **Esto sincroniza la línea de tiempo, no el audio.** El sonido sigue saliendo analógico por el
  PCM5102: para grabarlo necesitas interfaz de audio igual.
- **El reloj sale ~17 ms adelantado.** La cola del DMA hace que el código vaya por delante de lo
  que se oye, y es una constante. Se corrige con el **MIDI Clock Sync Delay** de Ableton en esa
  entrada, ajustando de oído.
- **Mientras se genera un sample el loop se bloquea varios segundos.** Se manda Stop antes y Start
  después, para no arrastrar el tempo del DAW. Genera los sonidos *antes* de armar la grabación.

## Los 3 botones como acorde

**Un slot vacío toca prestado el último sample cargado, pero transpuesto.** Con un solo sample
generado los tres botones ya te dan una **tríada menor** — si el sample es un Do, suenan Do · Re# · Sol:

| Botón | Intervalo | Multiplicador de frecuencia |
|---|---|---|
| BTN2 | tono original | `×1.000000` = 2^(0/12) |
| BTN3 | +3 semitonos (3ª menor) | `×1.189207` = 2^(3/12) |
| BTN4 | +7 semitonos (5ª justa) | `×1.498307` = 2^(7/12) |

El multiplicador es literalmente la razón de velocidad de lectura del sample (resampleo por
**interpolación cúbica**, ver más abajo), así que también estira/acorta la duración — es un pitch
de sampler, no un time-stretch.

En cuanto grabas un sample propio en ese slot deja de pedir prestado y suena a su tono original.
El LED del slot prestado se ve tenue en el color del slot que está usando. POT3 transpone todo el
conjunto por encima, así que el acorde se mueve entero manteniendo sus intervalos.

## Secuenciador

**32 pasos de semicorchea = dos compases.** A 120 BPM el patrón dura 4 s; a 60 BPM, 8 s. Con 16
pasos (un compás, 2 s) las texturas generadas de 3–5 s no alcanzaban a sonar antes de
re-dispararse. Si quieres más, es una sola constante: `SEQ_STEPS` en el `.ino`.

**Grabar:** con BTN5 sonando, cada vez que tocas BTN2/3/4 ese golpe queda escrito en el paso más
cercano (cuantizado). No hay modo de edición: tocas y queda. Mantener BTN5 borra todo.

**Los slots latcheados en loop se respetan:** el secuenciador no los re-dispara, así puedes
sostener una textura en BTN2 mientras el patrón golpea con los otros dos. Sin esto, cada paso
cortaba la textura que estabas sosteniendo a propósito.

El slot destino de la próxima grabación avanza solo (1 → 2 → 3 → 1) y se ve respirando en blanco
en los LEDs 0–2.

## LEDs

Los 6 SMD de la placa y la tira externa **comparten la línea de datos (GPIO 46)**: los SMD van
primeros y la tira de 80 se encadena después (86 en total).

### Los 6 SMD = indicadores de estado

| LED | Significado |
|---|---|
| 0–2 | Slots: apagado vacío · color cargado · flash al disparar · respirando en loop |
| 3 | Secuencia (pulso en cada negra) |
| 4 | Filtro IMU (color = cutoff, brillo = resonancia) |
| 5 | Estado: **verde** listo · **rojo** grabando · **ámbar** procesando · **magenta** error |

En modo mixer los LEDs 0–2 pasan a ser VU de los faders y el 5 se pone violeta.

### La tira de 80 = el show

Se reparte en **3 bandas de 26 LEDs, una por canal del mixer**. Todo lo que pinta sale de estado
que ya existe — nada de animación decorativa que no signifique nada:

| Elemento | Qué muestra |
|---|---|
| Brillo de fondo de la banda | **Volumen de ese canal** (el mixer se lee a distancia) |
| Cometa desde el centro | Disparo de ese slot, intensidad según su volumen |
| Banda respirando | Loop enganchado |
| Banda parpadeando rápido | Loop **en espera del pulso** |
| Tono general | Filtro del IMU: cerrado = frío/violeta, abierto = cálido |
| Punto que recorre la tira | Cabezal del secuenciador (32 pasos → 80 LEDs) |
| Imagen congelada / estrobo | **Stutter**: la tira estrobea al ritmo del grano, igual que el audio |
| Barrido rojo / ámbar | Grabando / generando (se ve de lejos, no parece colgado) |

**Alimentación:** la tira va con **5 V externos** y la masa unida a la placa. 80 LEDs no salen del
regulador de la ESP32.

## Formato de audio y plan de ElevenLabs

`pcm_22050` exige plan **Pro o superior**. El firmware lo intenta primero y, si tu key lo rechaza,
**reintenta solo en `ulaw_8000`** — disponible en todos los planes (incluido **Starter**): 8 kHz,
8 bits, lo-fi. Se decodifica aquí mismo, sin librerías. Verás cuál se usó en el Monitor Serie.

Para saber de antemano qué te acepta tu key:

```bash
curl -s -o /dev/null -w "%{http_code}\n" \
  -X POST "https://api.elevenlabs.io/v1/sound-generation?output_format=pcm_22050" \
  -H "xi-api-key: TU_KEY" -H "Content-Type: application/json" \
  -d '{"text":"short metallic hit","duration_seconds":1}'
```

`200` = tienes PCM. Cualquier `4xx` = va por µ-law.

## Calidad del sample — de dónde venían los chasquidos

Los samples cortos sonaban con chasquidos y una aspereza rara. Eran **cinco** causas distintas
apiladas, no una:

**1. El one-shot se cortaba en seco al terminar.** `held` valía la duración entera del sample, así
que la voz llegaba al final y se mataba (`active = false`) **antes** de que los 30 ms de cola
llegaran a aplicarse nunca. Lo único que suavizaba el final eran los 2 ms grabados en el sample —
y con samples cortos disparados en semicorcheas, eso eran cuatro chasquidos por negra. Ahora la
**rampa de salida corre también para los one-shot** (ventana de 8 ms en muestras del slot, no fija:
las 256 muestras de antes eran 32 ms con µ-law y se comían la cola).

**2. El arranque era un escalón.** El recorte de silencio deja el sample empezando en pleno
transitorio, no en un cruce por cero. Ahora **el arranque se ancla a un cruce por cero**: se busca
hacia atrás (hasta 2 ms) la muestra más cercana a cero y se empieza ahí. Solo hacia atrás —
moverse hacia adelante se comería justo el ataque. Con eso no hay escalón que suavizar, y el fade
de entrada puede ser casi nulo (ver el aviso de abajo).

**3. Se agotaba el pool de voces.** Con 8 voces, el secuenciador a semicorcheas más los retriggers
a mano robaban voces todo el rato, y cada robo era un salto de amplitud. Ahora son **12**, y el
criterio de robo es `env` (la ganancia que realmente está saliendo) en vez de `amp` (la objetivo):
una voz recién disparada tiene `amp = 1` pero `env ≈ 0`, y robar esa no cuesta nada.

**4. Interpolación lineal = imágenes del espectro crudas.** Es un filtro pésimo, y con el fallback
`ulaw_8000` (8 kHz → 44.1 kHz, ×5.5 de sobremuestreo) se notaba muchísimo: **ese era el "ruido
raro"**. Ahora es **cúbica (Catmull-Rom)**, ~4 multiplicaciones más por voz y muestra.

**5. Continua y normalizado que subía el ruido.** Los samples de IA — y el µ-law del plan Starter
sobre todo — traen offset de continua: falsea el pico, se normaliza mal, y cada arranque y cada
corte se vuelven un escalón. Ahora:

- La **continua se quita primero**, antes de medir nada.
- El normalizado toma la ganancia **más conservadora** entre dejar el pico en ~0.9 FS y dejar el
  **RMS** en ~0.25 FS. Un sample casi vacío con un único transitorio se multiplicaba por 12 y lo
  que subía era el suelo de ruido. El tope sigue en ×12: quien evita amplificar ruido es el
  criterio de RMS, no un techo bajo.
- Los fades de borde pasan de lineales a **curva de coseno**. El problema del fade lineal es la
  esquina: la señal llega a cero pero su *pendiente* cambia de golpe, y eso ya se oye como un tic.
- **Bloqueador de continua a la salida**, por canal, antes del limitador.

### ⚠️ Los fades de borde son ASIMÉTRICOS — y tienen que serlo

Los dos bordes no son el mismo problema:

| Borde | Fade | Por qué |
|---|---|---|
| **Salida** | 5 ms | Nadie oye que la cola se suavice, y es lo que mata el chasquido del final |
| **Entrada** | **0.5 ms** | Aquí está el **ataque**, y en una batería el ataque *es* el sonido |

La primera versión de estos arreglos puso **5 ms en los dos bordes**, y eso se come entero el
transitorio de un bombo o una caja: el golpe pierde el pegue y suena flojo, como si empezara a
media altura. Se notaba sobre todo en **BTN2**, que es el único que toca el sample a su tono
original y completo (BTN3/BTN4 lo transponen hacia arriba, o sea más corto y más agudo).

Por lo mismo, la rampa de ataque de la voz bajó de 1 ms a **0.25 ms**: existe solo como red de
seguridad para el robo de voz, no para suavizar el arranque — de eso se encarga el anclaje a cruce
por cero. 0.25 ms deja pasar entero hasta un armónico de 4 kHz.

Al cargar, el Monitor Serie imprime la medición completa:

```
Slot 1: 41300 muestras @ 22050 Hz | dc=-118 pico=9200 rms=1450 ganancia x3.21 | arranque en |37| (cruce por cero OK)
```

- `arranque en |...|` alto + `ATENCION: no encontro cruce` → el sample empieza de golpe sin nada
  antes; el fade de 0.5 ms igual lo cubre, pero es la señal de que ahí no hay margen.
- Ganancia pegada al tope con `rms` alto → el sample es ruidoso de origen. Pídelo otra vez con
  otra descripción.
- **`@ 8000 Hz` = estás en `ulaw_8000`** (plan Starter). Ahí una batería va a sonar apagada y
  pequeña por definición: son 8 kHz y 8 bits logarítmicos, no hay nada por encima de 4 kHz. Eso no
  se arregla en el firmware — es el plan de ElevenLabs.

## Antes de compilar

Copia `secretos.example.h` a `secretos.h` (misma carpeta del sketch) y pon ahí el WiFi y las API
keys `OPENAI_API_KEY` y `ELEVEN_API_KEY`. `secretos.h` está en el `.gitignore`: nunca se sube.
Librerías: FastLED (el resto viene en el core ESP32 ≥ 3.x).

### No te pelees con los menús: usa el perfil

Los cinco ajustes de placa están **fijados en [`sketch.yaml`](sketch.yaml)**, al lado del `.ino`.
El Arduino IDE 2.3+ lo ofrece como perfil en el selector de placa, y por línea de comandos:

```bash
arduino-cli compile --profile percusynth
```

Así no hay que acordarse de nada cada vez que el IDE se resetea. Si prefieres los menús a mano,
son estos cinco:

| Menú | Valor | Si está mal |
|---|---|---|
| **Placa** | `ESP32S3 Dev Module` | No compila nada (ver abajo) |
| **PSRAM** | `OPI PSRAM` | Arranca y parpadea magenta: ~1 MB de buffers no cabe |
| **Flash Mode** | `DIO 80MHz` | El I2S suena mal en este hardware |
| **USB Mode** | `USB-OTG (TinyUSB)` | El PC no ve ningún dispositivo MIDI (ver abajo) |
| **USB CDC On Boot** | `Enabled` | No ves el log del Monitor Serie |

### Si no compila: mira la placa antes que nada

Con la placa equivocada salen **dos errores a la vez que parecen no tener relación**:

```
error: 'USBMIDI' does not name a type
error: static assertion failed: Invalid pin specified
   note: '_ESPPIN<46, 16384, false>::validpin()' evaluates to false
```

Los dos son lo mismo: está compilando para **ESP32 normal en vez de ESP32-S3**. El ESP32 clásico
no tiene USB OTG (así que `USBMIDI.h` se auto-anula y queda vacío) y llega solo hasta el GPIO 39
(así que FastLED rechaza los pines 46 y 48 de los LEDs). El `.ino` lleva un `#error` que lo dice
directamente, pero el de FastLED puede aparecer primero en la lista.

### El USB Mode es una trampa silenciosa

Con `USB Mode: Hardware CDC and JTAG`, el sketch **compilaba y arrancaba sin quejarse — y el PC no
veía ningún dispositivo MIDI.** La clase `USBMIDI` existe igual en ese modo (viene del `sdkconfig`
precompilado del core, que no cambia con el menú), pero el USB lo está manejando el periférico
**USB-Serial-JTAG** en vez de la pila TinyUSB, así que `MIDI.write()` escribe a un stack que nadie
levantó.

Quien distingue los dos modos no es ninguna macro de TinyUSB sino **`ARDUINO_USB_MODE`**, que llega
como `-D` desde el menú: `0` = USB-OTG (TinyUSB), `1` = Hardware CDC and JTAG. El `.ino` lo revisa
y aborta con un mensaje explícito, así que este fallo ya no puede pasar callado.

### Si no necesitas el MIDI

Arriba del sketch, junto a los `#include`:

```cpp
#define MIDI_CLOCK_OUT   true    // ponlo en false
```

En `false` el firmware **compila con cualquier USB Mode** y no incluye nada de USB MIDI. Todo lo
demás — audio, secuenciador, LEDs, generación por voz — funciona igual; solo dejas de mandarle
reloj al DAW. Los puntos de llamada siguen ahí como funciones vacías, así que no hay `#ifdef`
regados por el código.

## Si algo "no suena"

El Monitor Serie a 115200 dice exactamente qué pasó. Cada botón imprime a qué slot va a sonar
(`BTN slot 2 -> suena slot 1` = está prestando), y BTN5 imprime `SEQ PLAY | 120 BPM | golpes: 0`.
Un patrón con **0 golpes** es 16 pasos de silencio: es lo esperado hasta que grabes algo encima.

## Notas de diseño

- **El disparo es instantáneo** (flanco de presión, cola DMA de 128 frames ≈ 17 ms). El "mantener"
  solo *agrega* el loop encima de algo que ya sonó — nunca hay una ventana de espera.
- **Al llegar, cada sample se limpia de continua, se recorta y se normaliza.** Los sonidos de IA
  suelen traer silencio inicial y nivel bajo; sin esto el botón se siente blando. Detalle completo
  arriba, en *Calidad del sample*.
- **Durante la generación el audio se detiene** (WiFi + TLS + espera de la API son varios segundos).
  La secuencia se pausa y se reanuda sola al terminar.
- **Los slots se pierden al reiniciar** — no hay microSD en el pinout. Volcarlos a LittleFS queda
  pendiente para una v2.

## Pendiente (v2)

- Persistir slots en flash (LittleFS) para que sobrevivan al reinicio
- Elegir slot destino a mano (BTN1 + BTN2/3/4) en vez de round-robin
- Aprovechar `loop:true` de la API para texturas con costura perfecta en vez del fade local
- **Robo de voz con fast-kill.** Con 12 voces casi no pasa, pero cuando pasa la voz robada sigue
  cortándose en seco. El patrón de `cyber_kit` (rampa de 4 ms) lo arregla sin perder el disparo
  instantáneo.
- **Recibir clock en vez de mandarlo.** Se puede (`USBMIDI::readPacket` existe), pero POT1 dejaría
  de mandar el tempo y el cuadrado de los loops al pulso tendría que estimar el BPM del clock
  entrante. Hoy el PercuSynth es maestro.
