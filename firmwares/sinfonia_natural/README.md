# sinfonia_natural — bosque + música en una escala celestial

Agua que corre, aves que cantan, campanitas y una música que acompaña, todo dentro de **una sola escala**:
**E · F# · G# · A · B · D#** (Mi mayor sin el 6º grado, hexatónica). Hasta las aves y las gotas cantan en ella.
No usa microSD ni IMU: todo vive en la placa.

Hoja de controles para imprimir o compartir: [controles_sinfonia_natural.pdf](controles_sinfonia_natural.pdf)
(se arma desde `controles.html` imprimiendo a PDF, A4 apaisado).

**Instalar desde el navegador (Chrome/Edge, sin Arduino IDE): https://gclabchile.com/sinfonia/** — ESP Web Tools con
un solo binario en la dirección 0 (`web/public_html/sinfonia/firmware/sinfonia_natural.bin`): la imagen fusionada que deja
`arduino-cli compile --output-dir` (`*.merged.bin`, FQBN `esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=dio`, sin PSRAM para
que sirva en cualquier placa) recortada de su relleno `0xFF` final, 4 MB → 1.1 MB. La página también ofrece el PDF y el `.ino`.
Si cambia el firmware hay que regenerar ese binario y volver a subir la carpeta `sinfonia/` (scp a `gclab-hostinger`).

## Controles

| | |
|---|---|
| **BTN1** | Play / Stop. Play entra con una campanita en la tónica; Stop apaga todo en ~2 s |
| **BTN2** | Escena nueva: **otros instrumentos** (una de 5 paletas, nunca la que sonaba) + otra progresión, tempo, melodía y aves. Lo anterior se va rápido y lo nuevo entra en el acto |
| **BTN3** | Canta un ave cerca (silbo, frase, trino o gorjeo), en la escala. Si está sonando, otra le contesta |
| **BTN4** | Cascada de campanitas sobre el acorde que suena |
| **BTN5** | Un animal grabado, uno distinto cada vez (búho, carpintero, chochín, grillo, mirlo, pájaros, paloma, ranas, zorzal) |
| **POT1** | Volumen |
| **POT2** | Balance MÚSICA ↔ NATURALEZA: a la izquierda sólo música, a la derecha sólo bosque, al centro las dos |
| **POT3** | DENSIDAD: cuántos instrumentos y cuántas notas |
| **POT4** | VERTIENTE grabada, sonando continua (abajo del todo, apagada). Tiene su propio volumen, no depende de POT2 |

BTN3, BTN4 y BTN5 suenan también con la música parada.

## La densidad (POT3), como en industrial_matriz

Abajo del todo sólo está el bosque (agua sintetizada + viento). Al subir entran, en este orden:

pad → bajo → campanitas → aves → arpegio → melodía → percusión aguda → tambor → gotas afinadas → coro de aves

Cada capa con patrón guarda una **prioridad por paso**: subir la perilla agrega notas en orden de importancia y
**nunca cambia las que ya sonaban**.

## Las paletas (BTN2)

Cambiar sólo la progresión casi no se notaba: cada escena trae sus propios instrumentos.

| Paleta | Pad | Melodía | Arpegio | Percusión aguda |
|---|---|---|---|---|
| BOSQUE | coro | flauta | arpa de cuerda | kalimba |
| CRISTAL | vidrio, una octava arriba | celesta | caja de música | vidrio |
| NOCTURNO | cuerdas oscuras | ocarina | marimba | kalimba |
| AMANECER | cuerdas brillantes | silbido | arpa de cuerda | vibráfono |
| AGUA | coro | clarinete | vibráfono | caja de música |

La primera escena es siempre BOSQUE. El Monitor Serial imprime la paleta y la progresión de cada escena.

## La música

- **Acordes que existen enteros dentro de la escala** (sin C#): Emaj9, Amaj7#11 (el lidio que flota), G#m7, B7sus4,
  F#11, E/G#, Badd11, Emaj7sus2. 8 progresiones lentas que no resuelven (sin V→I); cada acorde dura 2 compases.
- **Melodía**: un motivo de 2 compases (ritmo escrito como texto, `x` nota · `-` sigue · `.` silencio) que se repite
  sobre cada acorde y se re-armoniza solo: tiempos fuertes y notas largas caen en notas del acorde, el 2º y 4º acorde
  varían una nota, el ciclo cierra en la fundamental.
- **Arpa de cuerda**: Karplus-Strong afinado con un pasa-todos fraccional. Las otras percusiones (marimba, caja de
  música, vibráfono, vidrio, kalimba) son 3 parciales con razones **enteras**: nada desafina contra la escala.
- **Campanitas**: parciales armónicos largos (1, 2, 3) + un golpe inarmónico (4.2) que muere en una fracción de segundo.
  Muchas campanas inarmónicas largas a la vez se suman en un lavado disonante (lección de `la_partida`).
- **Aves**: 4 especies sintetizadas cuyas notas son grados de la escala entre E6 y B7.
- **Agua sintetizada** (suena como lluvia): ruido por 3 paso-banda que se mueven + retumbe grave + burbujas.
- **Espacio fijo**: reverb FDN de 4 líneas (cola oscura de 5.5 s) + eco ping-pong de **tiempo fijo** (0.33 s; un eco
  que sigue al tempo desafina al cambiar de escena). Antes estaba en el POT4, pero no se notaba.

## Sonidos grabados: todo en un solo .ino

El firmware es **un solo archivo**: los sonidos van escritos dentro del mismo `sinfonia_natural.ino` (el índice
arriba, los datos al final, entre las marcas `>>> SONIDOS` / `<<< SONIDOS`), así se le puede pasar a cualquiera sólo
el `.ino` y carga sin errores con la partición por defecto, sin microSD ni archivos extra. El `.ino` pesa ~2.2 MB.

Van comprimidos en **IMA ADPCM de 4 bits** (0.63 MB de flash en vez de 2.5 MB), en bloques de 1024 muestras con su
propia cabecera para poder empezar a leer en cualquier bloque. Medido sobre estos sonidos: 20–43 dB de señal/ruido
(la vertiente 20 dB —ruido sobre ruido—, los pájaros 21–28, la paloma 43), con el ruido de la compresión siguiendo a la
señal; el decodificador del firmware da exactamente esos valores.

Todos son **grabaciones de campo CC0** de Freesound (autores y enlaces en [fuentes_sonidos.txt](fuentes_sonidos.txt);
los recortes originales en `sonidos/`):

- **9 animales para BTN5**, 1.8–3.2 s cada uno. De cada grabación se tomó una frase completa que termina en silencio,
  buscada en la banda de cada animal (el búho en 250–1000 Hz: en banda completa ganaba el siseo de la noche). Todos
  quedan con el mismo nivel percibido (RMS −20 dBFS, pico ≤ −1 dBFS). Sin lluvia, viento ni agua: esos sonaban a lluvia.
- **La vertiente del POT4** (`continua_vertiente.wav`, 6 s). Una grabación así no puede sonar en bucle porque la vuelta
  se oye: cada canal lee trozos de 1–2 s tomados al azar (cada uno empieza en un bloque ADPCM) y los empalma con un
  fundido de potencia constante de 0.35 s, y L y R eligen trozos distintos. Suena continua, ancha y nunca igual.

`recortar.py` rehace los recortes desde una grabación larga (ver su cabecera). Para cambiar los sonidos, deja otros
audios en `sonidos/` (o quita alguno) y corre:

```bash
python preparar_muestras.py sonidos
```

Eso los comprime y los **reescribe dentro del `.ino`**. Un archivo que empieza con `continua_` va al POT4; los demás, al
BTN5. Caben ~38 s en total con la partición por defecto (22 KB por segundo). Los arreglos se escriben en líneas cortas:
con una sola línea gigante, el generador de prototipos del IDE falla («line too long»). `--borrar` saca los sonidos
(BTN5 hace gotas afinadas y POT4 no suena). También se pueden pedir a ElevenLabs (tu clave en una variable de entorno,
nunca en un archivo):

```powershell
$env:ELEVENLABS_API_KEY = "tu_clave"; python generar_elevenlabs.py
```

## Arduino IDE

ESP32S3 Dev Module · USB CDC On Boot: Enabled · Flash Mode: **DIO** · Partition Scheme: la de por defecto.
Librería: **FastLED**. La carpeta tiene que llamarse `sinfonia_natural`, como el `.ino`.

## Tira de luces atmosféricas (49 LEDs)

Una tira WS2812 de 49 LEDs encadenada **después** de los 6 LEDs de la placa, en el mismo GPIO 46 (`TIRA_LEDS` para otro
largo; sin tira el firmware funciona igual). Luces **calmas y lentas que no siguen a la música**: siete efectos que se
turnan solos — aurora · atardecer · olas · luciérnagas · claros del bosque · luna · nubes —, cada uno 45–80 s, con un
fundido de 8 s al siguiente y nunca el mismo dos veces seguidas; cada encendido parte con uno distinto. Entra con Play y
se apaga con Stop (1.5 s). El Monitor Serial dice qué efecto entra.

Cada efecto es una función del tiempo y de la posición (nada de azar por cuadro), así que no salta. Gamma 2.2 y
*dithering* temporal (se arrastra el resto de cada cuadro) para que los fundidos lentos no se vean a escalones.
**Limitador de corriente** de 400 mA (`TIRA_MAX_MA`): una tira colgada de los 5 V de la placa que prende muchos LEDs a la
vez hunde la alimentación y corta el audio (lección de `bajo_8_pasos_fsr_clock`); ataca en el acto y se recupera lento.
Con fuente propia se puede subir. Medido en el PC durante 20 min: los 7 efectos aparecen, 53–82 s entre cambios, corriente
máx. 399 mA (media 164), salto máximo entre cuadros 2.9 % del brillo, tira en 0 después de Stop.

## Pantalla OLED: un zorzal

En el conector OLED de la V2.0 (SSD1306 128×64, I2C 0x3C, SDA 21 / SCL 38; opcional, sin pantalla todo funciona igual)
vive un **zorzal posado en una rama**. Tocando está despierto y parpadea cada tantos segundos; cuando canta el ave de
**BTN3** abre el pico con cada una de sus notas y le salen notas musicales que suben flotando (a lo más ~4 por segundo,
para que un trino no suelte una nube). Parado, **duerme**: ojo cerrado, unas «z» que suben y la pantalla más tenue;
BTN3 lo despierta mientras canta. Sin librería de pantalla: sólo `Wire`, en su propia tarea del core 0 con la prioridad
más baja (un cuadro tarda ~25 ms por I2C a 400 kHz y el audio nunca lo espera); si se desconecta, la vuelve a buscar
cada 3 s.

Los tres cuadros (reposo · parpadeo/dormido · canta) los dibuja `generar_ave.py` —silueta a 4× umbralizada, y ojo,
ala, plumas y motas del pecho a 1 px— y los escribe **dentro del `.ino`** entre `// >>> AVE` y `// <<< AVE`;
`ave_vista_previa.png` los muestra sin cargar nada. Las notas y las «z» son glifos chicos que el firmware dibuja encima.

## LEDs de la placa

- **0..4**: la densidad, musgo → agua → cielo → oro → crema.
- **5**: verde que late con el pulso (tocando) · blanco tenue (parado) · **magenta** = el audio pasó del 80 % de su
  tiempo por bloque (si lo ves, avisa: el Monitor Serial imprime el pico de CPU cada 10 s).
- Destellos: celeste = ave · dorado = campanitas · aguamarina = animal · blanco = escena.

## Verificado

Probado copiando **sólo el `.ino`** a una carpeta vacía y compilando con la partición por defecto (con y sin PSRAM). Cargado en la placa: arranca limpio. Simulado en el PC con mocks
durante 150 s (Play, densidad 0 → 1, los cinco botones, las 5 paletas, balance y vertiente de punta a punta, Stop,
botones con la música parada, todo al máximo): 0 NaN, 0 muestras en el techo, **0 de ~735 000 notas fuera de la
escala**, la cola se apaga sola después de Stop; con todo al máximo el limitador baja como mucho 2.7 dB.
**Falta medir el CPU en la placa** (LED 5 / Monitor Serial).
La pantalla: cargado en la placa, la encuentra y arranca limpio («Pantalla OLED: el ave está posada» en el Monitor Serial).
