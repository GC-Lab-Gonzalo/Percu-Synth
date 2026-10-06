# industrial_matriz

Dos máquinas al mismo reloj. La **base** es un bombo a negras y un bajo potente. La **fábrica**
son 5 pistas de sonidos industriales que corren en paralelo con su propio largo. Los visuales
salen en una **matriz WS2812 de 32×8**.

## Conexión

- La matriz va **encadenada después de los 6 LEDs de la placa**, en el mismo pin (GPIO 46): el
  DIN de la matriz se conecta a la salida de datos de la placa.
- **Aliméntala aparte** con 5 V (4 A o más), con GND común con la placa y un condensador de
  1000 µF en su entrada. Con 256 LEDs colgados del 5 V de la placa se hunde el riel y se corta
  el audio. El firmware trae un limitador de corriente (`MATRIZ_MAX_MA`, 450 mA por defecto,
  seguro para USB). Con fuente propia súbelo a 2500.

## Prueba de orientación (los primeros 3 s)

| Debería verse | Si no |
|---|---|
| ROJO arriba a la izquierda, VERDE arriba a la derecha, AZUL abajo a la izquierda | `MATRIZ_ESPEJO_X` / `MATRIZ_ESPEJO_Y` |
| Una columna blanca que barre de izquierda a derecha | Si se ve como puntos sueltos o una fila: `MATRIZ_POR_COLUMNAS` |
| La columna entera y recta | Si zigzaguea, columna por medio invertida: `MATRIZ_SERPENTINA` |

Las cuatro constantes están juntas, arriba del `.ino`.

## Controles

| | |
|---|---|
| BTN1 | Play / Stop (arranca en el "1") |
| BTN2 | Nuevo patrón de percusión industrial (golpes, largo del ciclo y afinación) |
| BTN3 | Línea de bajo y figura del arpegio nuevas (los acordes y el keys van en el menú) |
| BTN4 (mantener) + POT1 | **Tempo** (70–170 BPM). Cambia desde donde estaba, sin saltos: el recorrido entero del pot son 120 BPM. Al soltar BTN4, el volumen se queda donde estaba hasta que el POT1 vuelva a pasar por esa posición |
| BTN5 | **Break**: mientras lo mantienes se callan bombo y bajo y lo demás sigue; al soltar vuelven a tiempo. Mantenido **más de 2 s** empieza la **subida** (ver abajo) |
| **BTN2 + BTN4** | Entra y sale del **menú de bancos** (ver abajo) |
| POT1 | Volumen |
| POT2 | Mezcla base ↔ resto (al centro suena todo a pleno) |
| POT3 | Corte del filtro del bajo (la resonancia va atada al corte) |
| POT4 | Densidad de la percusión: al subirlo se agregan golpes y los que ya sonaban se quedan |

### Menú de bancos

BTN2 + BTN4 entra al menú: el LED 5 parpadea ámbar. Adentro:

| Botón | Banco | Escala |
|---|---|---|
| BTN1 | Em – C | Mi eólico |
| BTN2 | Cm – B♭ | Do eólico |
| BTN3 | Bm – G | Si eólico |
| BTN4 | E – F | Mi frigio dominante |
| BTN5 | El siguiente patrón del keys (5) | |

Dentro del menú, POT1 es el **ataque del bajo** (1 ms – 150 ms), POT2 su **caída** (40 ms – 1.2 s)
POT3 la **variación del patrón de acordes** y POT4 el **oscilador continuo**: dos sierras con un
detune leve (±6 cents) y reverb, para tocar melodías encima. La posición del pot es la altura, sin
escalones (primero fue cuantizado a la escala y Gonzalo lo prefirió continuo): barre dos octavas
cuyas dos puntas son la tónica del banco (en la octava 3 y dos más arriba), así la nota más grave y
la más aguda caen en la escala. Cada punta tiene un 2 % de tope donde la nota es la tónica exacta.
El timbre se mueve con un **pasa-bajos resonante modulado** por un LFO libre de velocidad
aleatoria: cada 1,5 a 6 s sortea una velocidad nueva entre 0,08 Hz (un barrido de 12 s) y 9 Hz (un
trino rápido) y se desliza hacia ella en ~1 s, así pasa de muy lento a muy rápido sin saltos. Primero
fue amarrado al tempo y Gonzalo lo prefirió suelto. La resonancia va atada al corte (alta cerrado,
baja abierto) para que no quede un pico chillón arriba; ajustes en `OSC_CORTE_MIN`, `OSC_LFO_*` y
`OSC_Q_*`. Para que suene espacial, cada sierra va cargada a un lado (el batido del detune se abre en
estéreo), pasa por un **chorus estéreo** propio (12 ± 4 ms, LFO de 0,35 Hz, una lectura por lado) y
tiene un envío fuerte al **delay ping-pong** del keys, a corchea con puntillo (~0,5 dB bajo el seco).
Antes tenía reverb; Gonzalo la cambió por delay + chorus, y `OSC_REVERB` quedó en 0 por si se quiere
volver a probar. El primer 5 % del recorrido es silencio. La lectura del pot va suavizada y con una zona muerta de
~6 cents, para que el ruido del ADC no haga vibrar la nota. Fuera
del menú sigue sonando la última nota que dejaste, como el resto del panel: para callarlo, entra y
baja el POT4 del todo. Los ajustes están en las constantes `OSC_*` y `NIVEL_OSC`.

**Variaciones del POT3** (8 zonas; P = acorde principal del banco, S = su segundo acorde). Todas
mantienen la escala y el estilo: cambia cuánto dura cada acorde y entran acordes nuevos de la
escala. Si un acorde nuevo no existe limpio en la escala del banco, o repetiría a otro, se usa el
vecino consonante.

| POT3 | Patrón | Ciclo | En Em–C |
|---|---|---|---|
| 1 | P·6 S·2 (la de siempre) | 8 | Em … C |
| 2 | P·4 S·4 | 8 | Em … C … |
| 3 | P·3 S·1 | 4 | Em Em Em C |
| 4 | P·6 iv·1 S·1 | 8 | Em … Am C |
| 5 | P·4 S·2 VII·2 | 8 | Em … C C D D |
| 6 | P·2 S·2 P·2 iv·2 | 8 | Em Em C C Em Em Am Am |
| 7 | P·5 III·1 S·2 | 8 | Em … G C C |
| 8 | P·8 S·4 iv·2 S·2 | 16 | Em ×8 C ×4 Am Am C C | Lo que dejes ahí se queda al salir. Al entrar o salir,
cada pot no cambia nada hasta que lo muevas un poco, así ni el volumen ni el bajo saltan.

### La subida (BTN5 mantenido más de 2 s)

1. Bombo y bajo se callan; percusión, keys y arpegio siguen.
2. Durante **16 compases** crecen un **ruido blanco** y un **oscilador agresivo** (dos sierras
   desafinadas y saturadas) que parte en la tónica grave del banco y sube 4 octavas.
3. Desde el **compás 9** se les suma un **trémolo rítmico** en semicorcheas, amarrado a la grilla
   de la música, hasta el final (frenarlo le quitaba el efecto), y entra una **caja** que crece:
   negras (9–10) → corcheas (11–12) → semicorcheas (13–16), cada vez más fuerte y más corta.
4. Termina en el **"1"** de un compás con un **golpe** (boom grave que cae + crash de ruido) que
   deja una **cola de reverb corta** (~1.5 s) sobre el silencio: todo lo demás se corta de golpe.
5. El próximo toque de BTN5 es el **drop**: todo vuelve desde el "1" con bombo y bajo. Si lo
   tocas antes de que termine la subida, el drop entra en ese momento.

LEDs 0 y 1: rojo parpadeando durante la subida, rojo fijo esperando el drop. Los ajustes están en
las constantes `SUBIDA_*`.

El acorde principal manda y el segundo aparece para evitar la monotonía; cuánto dura cada uno lo
elige la variación del POT3 (abajo). Los LEDs 0 a 3 muestran el banco
y el LED 4 el patrón del keys, con un color por patrón. La música no se detiene mientras eliges.
Se sale con BTN2 + BTN4, y el combo deshace lo que alcanzó a hacer el primero de los dos botones.

## Sonido

- **Bombo**: el de tres bandas de `trance_pistas`/`drum_poder`.
- **Armonía**: los 4 bancos de dos acordes del menú. La tonalidad cambia con el banco y el bajo,
  el arpegio y el keys la siguen.
- **Bajo**: el de `bajo_8_pasos`, con más drive. Sigue la fundamental de cada acorde y ninguna de
  sus 6 líneas toca en una negra. Su séptima es siempre menor; si el acorde trae séptima mayor,
  toca la quinta (la séptima mayor quedaba medio tono bajo la fundamental y sonaba desafinada).
- **Keys**: monofónico (nunca suenan dos notas a la vez). Un oscilador de **pulso con PWM**: un
  LFO lento le va cambiando el ancho y el timbre respira. Pasa por un pasa-bajos con envolvente,
  después por un **chorus estéreo** (retardo corto modulado en cuadratura, uno a cada lado) y por
  un **eco ping-pong a corchea con puntillo** que sigue al tempo. Cada repetición del eco se
  oscurece y cruza de lado; si el tempo cambia, el eco pasa al largo nuevo con un cruce de 50 ms en
  vez de deslizar el cabezal, que desafinaría las repeticiones. La tónica queda siempre entre Do4 y
  Si4. Toca uno de 5 patrones escritos, hipnóticos y oscuros, con ritmo y notas juntos y relativos
  al acorde: pulso con la sexta que cae a la quinta, la tercera insistente, descenso lento,
  semicorcheas y una nota larga con respuesta. Gonzalo los eligió en la placa; descartó el
  arpegio a contratiempo, la síncopa 3-3-4-3-3 y el dub. Los
  ajustes del timbre y de los efectos están en un bloque de constantes (`KEYS_*`, `CHORUS_*`,
  `ECO_*`).
- **Arpegio**: notas del acorde con el ritmo de los hats que suenan; la figura sólo pone el orden.
- **Percusión industrial**: HAT · MARTILLO (golpe sobre plancha) · PISTÓN (golpe hidráulico grave
  con escape) · CLAP (un solo golpe) · HAT ABIERTO · TOM. El ciclo dura 16 o 32 pasos y su posición
  se calcula siempre desde el paso de la base, así que cambiar de patrón nunca lo corre de tiempo.
  Antes había ciclos de 10, 12, 14, 20 y 24 pasos (un polirritmo): como 64 no es múltiplo de ellos,
  el ciclo se cortaba a la mitad cada 4 compases y cada patrón nuevo arrancaba en una fase
  cualquiera. Se oía como bajo y batería que no calzaban.
- **Descartado por sonar mal**: parciales metálicos afinados (sonaban a olla y a balón de gas),
  ráfagas de clics de ruido (sonaban a clipeo) y ráfagas de ruido que crecen (sonaban a ola).
- El bombo agacha al bajo, al pluck, al arpegio y a la percusión (sidechain). El master pasa por un bloqueador
  de DC, un limitador con lookahead y un pasa-bajos de 13 kHz.

## Matriz

Hay 8 escenas psicodélicas. Cada 4 compases (y en cada Play) se sortea una, nunca la misma dos
veces seguidas, y con ella se sortean la paleta, el sentido de giro y la forma, así que la misma
escena nunca se ve igual:

TÚNEL · PLASMA · CALEIDOSCOPIO · ESPIRAL · FIGURAS · MOIRÉ · ONDAS · DAMERO

Todo se mueve en tiempo musical, amarrado a la negra. El bombo ensancha las líneas, agranda las
figuras e invierte el damero. La envolvente real del bajo abre los contornos del plasma, y cada
golpe industrial suelta su figura (círculo, cuadrado, rombo o triángulo) o mueve su onda. Con la
máquina detenida la escena sigue, tenue y lenta.

Estas escenas encienden muchos LEDs a la vez. Con el tope de 450 mA el limitador las deja entre el
20 y el 40 % del brillo. Con fuente propia para la matriz, sube `MATRIZ_MAX_MA` y se ven a pleno.

## LEDs de la placa

0 bombo · 1 bajo · 2 hats · 3 martillo/pistón/clap/tom · 4 pluck/arpegio · 5 verde con el pulso de
cada negra. Durante el break, 0 y 1 quedan en rojo. Si el LED 5 se pone **magenta**, un bloque de
audio usó más del 80 % de su tiempo. En ese caso el problema es de CPU y no de señal.

## Tiempo de CPU (medido en la placa)

Un "clipeo" que aparece al sumar capas casi nunca es de nivel: es el audio que no alcanza a
calcular su bloque, el DMA se vacía y el DAC suelta ceros. Para separar las dos cosas está
`MEDIR`. Compilando con `-DMEDIR=1` la placa arranca tocando sola en el peor caso (densidad al
máximo, ruido y sirena encendidos) y cada 2 s imprime por el conector UART (115200) el uso de CPU,
lo que cuesta cada sección y cuánto trabaja el limitador.

Lo que más ayudó:

- **`#pragma GCC optimize ("O2")`**: el core de Arduino compila con `-Os` (tamaño). Con O2 el
  mismo código pasó de 65 % de promedio y picos de 95 % a **42 % / 58 %**.
- **FastLED se inicializa en la tarea de LEDs (core 0)**: hecho en `setup()` quedaba en el core 1
  y sus interrupciones le robaban tiempo al audio.
- Voces justas: el keys es monofónico, el arpegio usa 2 voces, la percusión sólo calcula los
  parciales que usa y los efectos que cambian lento se calculan cada 16 o 32 muestras.

## Verificado

- Compila con `arduino-cli` para ESP32-S3: 395 KB.
- Simulado en el PC con mocks durante 160 s de uso: play/stop, patrones, líneas, break, tap,
  barrido de los 4 pots y el peor caso (todo al máximo).
- Resultados: 0 NaN, 0 muestras al tope (pico 0.97 en el peor caso), ninguna voz trabada
  después del stop, tempo dentro del 0.07 %, las 4 escenas aparecen, ningún frame de la matriz
  pasa de 450 mA y el mapeo de la matriz cubre los 256 LEDs una vez cada uno.
- **Falta probarlo en la placa con la matriz.**
