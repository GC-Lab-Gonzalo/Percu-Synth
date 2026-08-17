# SAMPLER IA — material para el blog de GC Lab Chile

> **Para qué es este archivo:** insumo completo para escribir la página de blog en la web de
> GC Lab. Contiene la historia real del desarrollo (con los errores incluidos), las decisiones
> técnicas y por qué se tomaron, cifras verificadas, e ideas para el componente interactivo.
> Todo lo que está aquí ocurrió de verdad en la sesión de desarrollo — no inventar cifras ni
> agregar datos que no estén en este documento.

---

## 1. Qué es el proyecto (el pitch)

**Un sampler al que le pides los sonidos hablando.** Mantienes un botón, dices *"quiero un golpe
metálico oxidado con cola larga"*, y el PercuSynth lo genera con IA, lo carga en un slot y te lo
deja disparable al instante. Sin computador: el ESP32-S3 habla directo con las APIs.

La cadena completa:

```
BTN1 (mantener) → mic INMP441 → Whisper (transcribe el español)
                → GPT-4o-mini (lo convierte en prompt de efecto de sonido EN INGLÉS,
                               y decide duración y si conviene que loopee)
                → ElevenLabs /v1/sound-generation (genera el audio)
                → recorte de silencio + normalizado + micro-fades → slot en PSRAM
BTN2/3/4 → disparo instantáneo · mantener = loop/textura · BTN5 → secuenciador de 32 pasos
```

Es la fusión de tres piezas que ya existían en el ecosistema PercuSynth:
- **asistente_ia** — la cadena de voz (mic → Whisper → GPT por WiFi)
- **sample_loader** — el motor de disparo (resampleo por pitch, envolventes, pool de voces)
- **trance_imu** — el filtro biquad resonante controlado por el IMU

Moraleja para el blog: *casi nada se escribió desde cero; el valor estuvo en combinar piezas
probadas y resolver lo nuevo (la generación en vivo).*

## 2. La historia, en orden real (con los tropiezos)

Esto le da honestidad al blog. El proceso NO fue lineal.

### 2.1 La pregunta equivocada que llevó a la respuesta correcta
La idea nació preguntando si se podía usar la **API de música** de ElevenLabs. Resultó ser la
herramienta equivocada: devuelve canciones enteras (mínimo 3 s, hasta 10 min, archivos de MB,
cerradas, sin control musical posterior). El descubrimiento clave fue que el endpoint correcto
para un sampler era otro: **`/v1/sound-generation`** (Text-to-Sound Effects) — de 0.5 a 30 s,
con flag de `loop` para texturas sin costura y salida en **PCM crudo** que entra casi directo al
I2S del ESP32, sin decodificar MP3.

### 2.2 El plan Starter y el fallback µ-law
El formato limpio (`pcm_22050`) exige plan Pro de ElevenLabs, y el plan disponible era
**Starter**. La solución: *fallback automático* a **`ulaw_8000`**, disponible en todos los
planes. µ-law es un formato telefónico de 8 bits que se decodifica en ~10 líneas de C, sin
librerías. Suena lo-fi (8 kHz), pero para percusión y texturas ese carácter hasta suma.
El firmware intenta PCM primero y cae solo a µ-law si la key lo rechaza.

### 2.3 Primer bug: el preprocesador de Arduino
Primer intento de compilar: `'SfxSpec' does not name a type`. Arduino genera los prototipos de
todas las funciones **al inicio del archivo**, antes de donde estaban declaradas las structs.
Solución: declarar las structs arriba del todo. Clásico de Arduino que todo maker se topa alguna
vez.

### 2.4 El 401 que no era lo que parecía
Primera prueba real: `401 Unauthorized` en ambos formatos. No era el plan ni el formato — la
API key **autenticaba bien pero le faltaba el permiso `sound_generation`** (las keys de
ElevenLabs tienen permisos por endpoint que se habilitan en el dashboard). La lección de
depuración: el motivo real venía en el **cuerpo** de la respuesta HTTP, y el firmware solo
imprimía la línea de estado. Se arregló la key *y* se mejoró el firmware para imprimir el cuerpo
del error — depurar a ciegas nunca más.

### 2.5 "No funciona nada" (el instrumento nacía mudo)
Con el primer sample generado (una explosión), reporte del tester: los botones 3 y 4 no suenan,
el secuenciador no suena, los pots no se sienten. **Todos los síntomas eran la misma causa**: un
solo sample cargado. Los slots 2 y 3 estaban vacíos (disparaban silencio "correctamente") y el
patrón del secuenciador estaba vacío (16 pasos de silencio). La peor forma de fallar es fallar
en silencio. Fixes:
- Un slot vacío **toca prestado** el último sample cargado.
- Diagnóstico por Serial en cada botón (`BTN slot 2 -> presta el slot 1 x1.189207`).
- El pitch pasó a aplicarse **en vivo** sobre lo que ya suena (antes solo al disparar, y por eso
  "no se sentía").

### 2.6 La tríada menor (idea de Gonzalo)
En vez de que los tres botones repitan la misma nota cuando hay un solo sample: **transponerlos
como acorde**. BTN2 = tono original, BTN3 = +3 semitonos, BTN4 = +7 semitonos → una tríada menor
(si el sample es un Do: Do · Re# · Sol). Implementado como multiplicadores de frecuencia sobre
la velocidad de lectura del sample:

| Botón | Intervalo | Multiplicador |
|---|---|---|
| BTN2 | original | ×1.000000 = 2^(0/12) |
| BTN3 | 3ª menor | ×1.189207 = 2^(3/12) |
| BTN4 | 5ª justa | ×1.498307 = 2^(7/12) |

Con un solo sample generado ya tienes un instrumento armónico. El pot de pitch transpone el
conjunto entero, así que el acorde se mueve manteniendo sus intervalos.

### 2.7 El POT4 que se rechazó (y está bien que así fuera)
El cuarto potenciómetro pasó por tres vidas: **largo del sample** (aburrido), **PWM
waveshaper** (convertía el sample en onda cuadrada — técnicamente interesante, musicalmente
rechazado por sonar a distorsión), y finalmente **stutter granular**, elegido entre tres
opciones presentadas (resonador/eco, ring mod, stutter). Congela un trocito de la mezcla y lo
repite; el grano se acorta exponencialmente de 500 ms a 2 ms a lo largo del recorrido, así que
al medio tartamudea rítmicamente y al máximo el grano se vuelve un **tono** (~500 Hz).
Tres detalles que lo hacen usable:
- Grano **exponencial** (en lineal, todo lo interesante se apelotona al final del pot).
- **Recaptura cada ~200 ms** (si no, el grano se queda pegado y deja de seguir a la música).
- **Ventana en los bordes** del grano (sin fade, a granos de 2 ms son cientos de clicks/s).

Moraleja para el blog: *iterar con el instrumento en la mano vale más que acertar a la primera.*

### 2.8 El chicharreo del loop: un bug de una línea
Los loops sonaban con un chicharreo en cada vuelta. Causa: al dar la vuelta, la posición de
lectura quedaba **negativa** (se restaba el largo una muestra antes de tiempo); al convertir ese
número negativo a entero sin signo, el índice apuntaba basura y la interpolación extrapolaba
ruido — una vez por vuelta. El fix: envolver la posición correctamente e interpolar la costura
contra la muestra 0. Un bug de aritmética de una línea que se oía perfectamente.

### 2.9 La cache envenenada (lección de herramientas)
En plena sesión, la compilación explotó con `bad reloc symbol index` en FastLED. No era código:
**arduino-cli y el Arduino IDE comparten el mismo directorio de cache de compilación** (indexado
por la ruta del sketch), y dos compilaciones simultáneas — una del asistente de IA en segundo
plano, otra del IDE — se pisaron los archivos objeto. Solución: borrar la cache y no compilar
en paralelo. Los offsets del error eran texto ASCII incrustado en los `.o`: la firma de dos
procesos escribiendo el mismo archivo.

### 2.10 Que los loops calcen: el problema musical más interesante
Los loops generados **siempre quedaban desfasados** del secuenciador. Pedirle la duración exacta
a la API no basta: no la respeta al milisegundo, y el recorte de silencio inicial vuelve a
cambiar el largo. La solución fue por tres capas:
1. **Generación**: la duración pedida se redondea a un número entero de negras al BPM actual, y
   si es textura el prompt incluye "seamless loop at N BPM".
2. **Reproducción**: la vuelta del loop se fija al múltiplo de negra más cercano — se trunca o
   se deja aire, pero **no se estira el audio** (estirar cambiaría el tono; un loop afinado que
   no calza es peor que uno que calza y pierde la cola). Rampa en la costura para no clickear.
3. **Entrada cuantizada**: el loop no arranca donde soltaste el botón, espera **a la siguiente
   negra** (como Ableton). Este era el desfase que más se notaba.

### 2.11 El mixer escondido en los botones
Con 5 botones y 4 pots no sobran controles. Solución: **paneles**. Mientras mantienes un botón
de disparo (el mismo gesto que engancha el loop), los pots 2/3/4 se convierten en faders de los
tres canales. El truco que lo hace usable es el patrón de **pots congelados** (heredado de
pads_imu y cyber_kit): al cambiar de panel, cada pot deja de mandar hasta que lo mueves, y cada
modo recuerda su valor — sin eso, al soltar el botón el volumen master pegaría un salto a donde
quedó el pot físico. Los LEDs se vuelven VU-metros mientras estás en el mixer.

### 2.12 La tira de 80 LEDs: luz que significa algo
Cierre: una tira WS2812 de 80 LEDs encadenada tras los 6 SMD de la placa (86 en la misma línea
de datos, GPIO 46). Principio de diseño: **todo lo que pinta sale del estado del instrumento**,
nada de animación decorativa:
- 3 bandas de 26 LEDs = los 3 canales; el brillo de fondo ES el volumen del canal.
- Cometa desde el centro de la banda = disparo.
- Banda respirando = loop enganchado; parpadeo rápido = loop esperando al pulso.
- Punto recorriendo la tira = cabezal del secuenciador.
- El tono general lo pone el filtro del IMU (cerrado = violeta, abierto = cálido).
- El **stutter congela y estrobea la imagen al ritmo del grano** — ves lo mismo que oyes.
- Grabando/generando = barrido rojo/ámbar visible de lejos.

## 3. Ficha técnica (verificada)

| Ítem | Valor |
|---|---|
| Micro | ESP32-S3 con PSRAM (OPI), Flash Mode DIO |
| Audio | DAC PCM5102 por I2S, 44.1 kHz / 16 bit estéreo |
| Mic | INMP441 por I2S, 16 kHz mono (para Whisper) |
| Slots de sample | 3 × hasta 5 s en PSRAM (~660 KB) |
| Voces de reproducción | 8 (con robo de voz) |
| Latencia de disparo | flanco de presión + cola DMA de 128 frames (~17 ms de buffer) |
| Secuenciador | 32 pasos de semicorchea (2 compases), overdub cuantizado en vivo |
| APIs | OpenAI Whisper + GPT-4o-mini · ElevenLabs Text-to-Sound Effects |
| Formatos | pcm_22050 (plan Pro) con fallback automático a ulaw_8000 (cualquier plan) |
| LEDs | 6 SMD estado + tira de 80 (86 en total, GPIO 46), tira con 5 V externos |
| Compilado | 1.099.xxx bytes ≈ 83 % de flash · ~49 KB ≈ 15 % de RAM estática |
| Filtro | Biquad LPF resonante (RBJ), IMU X → cutoff (200 Hz–12 kHz), Y → Q (0.7–7.7) |

## 4. Ganchos narrativos para el blog

- **"Le pides el sonido hablando y existe 15 segundos después."** El arco completo
  voz → IA → hardware sonando.
- **"El instrumento que nacía mudo"** — cómo un diseño técnicamente correcto puede ser una
  pésima experiencia, y cómo se arregló (préstamo de samples, tríada, diagnóstico visible).
- **"Depurar con los oídos"** — el chicharreo del loop era un bug de casting audible; el
  estrobo de la tira LED muestra el grano del stutter. En un instrumento, el sonido ES el log.
- **"Errores que enseñan"** — el 401 de permisos, la cache compartida, el preprocesador de
  Arduino. Todos reales, todos con moraleja concreta.
- **"Restricciones que suman"** — el plan Starter forzó el fallback µ-law 8 kHz… que suena
  lo-fi con carácter. La tríada menor existe porque solo había UN sample cargado.
- **Aprender haciendo** (es literalmente la metodología GC Lab): el POT4 se rechazó dos veces
  con el instrumento en la mano antes de encontrar el stutter.

## 5. Ideas para el componente interactivo de la página

Ordenadas por relación esfuerzo/impacto (todas factibles con Web Audio API, sin backend):

1. **Demo de la tríada por multiplicadores** ⭐ recomendada — tres botones en pantalla que
   reproducen el mismo sample a ×1.0 / ×1.189207 / ×1.498307. El visitante ESCUCHA cómo un
   multiplicador de velocidad se vuelve un acorde. Simple, directa, explica el concepto núcleo.
2. **Stutter granular interactivo** — un slider (el "POT4") sobre un loop de audio: tartamudeo
   → granulado → tono. Espectacular y es exactamente el efecto del firmware (buffer + repetición
   con ventana, ~30 líneas de Web Audio).
3. **Simulador de la cadena de generación** — diagrama animado voz → Whisper → GPT → ElevenLabs
   → slot donde cada etapa muestra su entrada/salida real (el pedido en español, el prompt en
   inglés generado, la forma de onda). Puede usar ejemplos pre-grabados, sin llamar APIs.
4. **El bug del loop, audible** — toggle "antes/después": el mismo loop con el wrap roto
   (chicharreo simulado) y arreglado. Enseña qué se oye cuando la aritmética está mal.
5. **Cuantización del loop visual** — un botón "latchear loop" sobre una grilla que corre: sin
   cuantizar entra donde apretaste (desfasado para siempre), cuantizado espera a la negra.

Recomendación editorial: elegir **una** (la 1 o la 2) y hacerla impecable, en vez de varias a
medias.

## 6. Notas de marca y estilo para la página

- Identidad web: usar **design-system v2** de GC Lab (`GC Lab Chile/design-system/`) — dark +
  chartreuse `#d4ff3a`; la paleta v1 (cian `#00d4ff`) está obsoleta.
- Tono GC Lab: técnico pero cercano — "construye", "experimenta", "aprende haciendo".
- NO usar look "osciloscopio dark-neón glow" como estética por defecto; colorido, pulido,
  profesional. Verificar el resultado renderizando.
- Micro-interacciones sí (reveals, tilt, magnetismo) — hay capa lista en
  `design-system/css/motion.css` + `js/motion.js`.
- **No inventar cifras ni testimonios.** Todo dato debe salir de este documento o confirmarse
  con Gonzalo.
- Código y firmware viven en el repo PercuSynth: `firmwares/sampler_ia/` (README técnico
  incluido). Licencias: MIT (software) + CERN-OHL-P (hardware) — mencionarlas suma al mensaje
  de código abierto.

## 7. Créditos y contexto

Desarrollado por **Gonzalo Sandoval — GC Lab Chile** en sesión de pair-programming con Claude
(Anthropic) como asistente de código: Gonzalo definió la visión, el mapeo de controles, probó
cada iteración en el hardware real y rechazó lo que no sonaba bien; el asistente implementó,
depuró y documentó. El ciclo completo — de la pregunta inicial sobre la API a un instrumento
funcionando con mixer, secuenciador y show de luces — ocurrió en una sola sesión iterativa
con hardware en la mano.
