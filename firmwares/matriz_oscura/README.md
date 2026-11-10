# matriz_oscura

El sucesor oscuro y agresivo de `industrial_matriz_v2`. Tiene la misma banda en la placa (batería,
bajo, melodías y oscilador, cada uno con su panel) y la misma lógica: las secuencias corren siempre y
todo se escribe en grados de la escala, así que nada sale de tono ni de tiempo. Lo nuevo son **8
estilos oscuros**, timbres nuevos, un **sexto panel de samples** y tres escenas hipnóticas más en la
matriz 32×8. Los samples son un **noticiero del futuro que se glitchea mientras informa**: 24 frases cortas con voces de
ElevenLabs y el glitch horneado. **Van dentro del mismo `.ino`**: se comparte ese único archivo y se carga
tal cual, sin tarjeta ni archivos aparte.

Los estilos se eligieron escuchando los clips de [`ejemplos/`](ejemplos/) (`generar_ejemplos.py`,
detalle en `ejemplos/estilos.txt`). En cada estilo, el **patrón 1** y el timbre que trae al elegirlo
son los aprobados; los otros tres patrones y los demás timbres son variaciones en esa dirección.

**La hoja de controles para imprimir** está en [`controles_matriz_oscura.pdf`](controles_matriz_oscura.pdf) (y su fuente, `controles.html`).

## Estilos (BTN2 del General)

| # | Estilo | Escala | Progresión | Kit · bajo · melodía · oscilador | Tempo sugerido |
|---|---|---|---|---|---|
| 0 | **INDUSTRIAL** | Mi eólico | Em ×6 · C ×2 (el del v2) | INDUSTRIAL · SUCIO · STAB · GRUÑIDO | 128 |
| 1 | **PSYTRANCE** | Mi frigio | Em ×7 · F | PSY · RODANTE · ÁCIDO · GRUÑIDO | 145 |
| 2 | **PSYCORE** | Fa# doble armónica (árabe) | F# ×4 · G ×2 · F# ×2 | PSY · RODANTE · FM · FM | 170 |
| 3 | **ELECTRO DARK** | Fa frigio | F ×2 · Gb · F | INDUSTRIAL · SUCIO · SYNC · GRUÑIDO | 124 |
| 4 | **INDUSTRIAL DURO** | Re frigio dominante (hijaz) | D (un acorde) | DURO · SUCIO · STAB · GRUÑIDO | 136 |
| 5 | **CYBERPUNK** | Do# dórico | C#m · B · F# · G#m | ROCK · LIMPIO · PLUCK · GRUÑIDO | 100 |
| 6 | **TRAP** | Fa dórico | Fm · Ab · Eb · Bb | TRAP · 808 · PLUCK · SIERRAS | 140 |
| 7 | **ROCK INDUSTRIAL** | Mi frigio dominante (hijaz) | E ×6 · F ×2 | ROCK · SUCIO · QUINTAS · SIERRAS | 118 |

El tempo no cambia con el estilo: lo pones tú con el POT2 del General (70–175 BPM).

- **Armonía:** eólico, frigio, dos escalas **árabes** (frigio dominante o hijaz, y doble armónica, con
  sus dos segundas aumentadas) y dórico. Los acordes se apilan en terceras de la escala tal cual y las
  progresiones sólo usan grados con tríada consonante. Al principio PSYCORE, INDUSTRIAL DURO y ROCK
  INDUSTRIAL eran locrios, y CYBERPUNK y TRAP iban en menor armónica: se cambiaron a pedido después de
  la primera prueba. Los patrones están escritos en grados, así que se recolorearon solos; en el trap,
  la respuesta del 6d ahora pasa por la sexta mayor del dórico (Re natural sobre Fa).
- **Melodías:** el alfabeto suma el `0`, el grado de abajo del acorde (la séptima por debajo). Lo
  necesita el trap, cuya respuesta baja del Do al Re♭ sobre el Db.
- **Bajo:** el alfabeto es `x 2 3 4 5 6 b o`, en grados sobre la fundamental del acorde.
- **Batería:** se suman `r` (redoble de 2 golpes en el paso) y `t` (de 3). Así se hacen los redobles
  del hat del trap y del psycore. Cada golpe del redoble cae en su muestra exacta, un poco más suave
  que el anterior.
- **Rock industrial:** la melodía es el riff de quintas, dos octavas abajo (`octKeys = -2`), y el
  bajo lo dobla.

## Controles

Son los de `industrial_matriz_v2` más el panel 6. En todos los paneles, **BTN4 = siguiente panel** y
**BTN5 = break**. Mantenido más de 2 s, el BTN5 hace la subida; el próximo toque es el drop. Al cambiar
de panel, cada pot toma el control recién cuando lo mueves un 4 %.

| Panel (LED 5) | BTN1 | BTN2 | BTN3 | POT1 | POT2 | POT3 | POT4 |
|---|---|---|---|---|---|---|---|
| General (verde) | play/stop | estilo | fill al final del compás | volumen | tempo 70–175 | swing | pasa-bajos general |
| Bajo (rojo) | **distorsión** sí/no | patrón | timbre: RODANTE · SUCIO · LIMPIO · 808 · ÁCIDO · PULSO | volumen | filtro | **resonancia** | caída (el 808 ×3.5) |
| Batería (ámbar) | **percusión extra**: 1 → 2 → 3 pistas → nada | patrón | kit: INDUSTRIAL · PSY · DURO · ROCK · TRAP | volumen | filtro | largo de los golpes | densidad |
| Melodías (cian) | **octava**: normal · +1 · −1 | patrón | timbre: ÁCIDO · FM · STAB · SYNC · PLUCK · QUINTAS | volumen | filtro | **resonancia** | caída |
| Oscilador (azul) | on/off | timbre: GRUÑIDO · SIERRAS · FM · CUADRADAS | octava | **nota** | filtro | LFO | **volumen** |
| **Samples** (violeta) | dispara un **TITULAR** | dispara al **PRESENTADOR** | dispara una **SEÑAL CAÍDA** | volumen | pitch | reverb | eco |

Con el 808, el bajo se desliza entre notas. El glide se prende solo al elegir ese timbre.

**Cambios después de probarlo (2026-11-02):** los instrumentos ya no tienen play/stop (para callar uno
se baja su volumen) y su BTN1 hace otra cosa:

- **Bajo:** prende y apaga la **distorsión**. Multiplica ×3,2 la saturación del timbre, siempre antes del
  filtro y con el nivel compensado: suena más sucio, no más fuerte.
- **Batería:** suma **percusión extra** hecha de **recortes de los samples**, 12 trozos de 40 a 260 ms
  sacados del golpe más fuerte de metales, FX e impactos, con su propia caída. Cada toque suma una pista:
  la 1 usa recortes cortos (40–60 ms), la 2 medios (80–120) y la 3 largos (160–260); el cuarto toque las
  apaga.
  - **Dónde caen:** sólo en los huecos del patrón (donde no hay bombo, caja ni clap), así que no pisan lo
    escrito. Cada pista toca sus huecos de mayor prioridad: 4, 3 y 2 por compás con la densidad al
    máximo, menos al bajarla.
  - **Qué recorte suena:** cada pista tiene uno fijo para cada estilo y patrón.
- **Melodías:** cambia la **octava** (normal → +1 → −1).

El POT3 de Bajo y Melodías pasó de ataque a **resonancia**, y el ataque quedó fijo (2 y 3 ms). En el bajo
la resonancia baja sola cuando el filtro se abre, para que nunca haya un pico chillón arriba.

**Oscilador:** en la primera prueba se oía muy despacio y no tenía volumen. Ahora el POT4 es su volumen y
el envío al delay quedó fijo (`OSC_ECO_FIJO`). Medido por RMS con el POT4 a 0.75, suena unos 5–6 dB
sobre las melodías; al máximo, 6 dB más. El compresor propio lo sigue dejando parejo entre timbres.

### Timbres

- **Bajo.**
  - RODANTE: el del psy, corto y con el filtro que se cierra al tiro.
  - SUCIO: sierra más una cuadrada una octava abajo.
  - LIMPIO: dos sierras a 7 cents. Es el de las octavas del cyberpunk.
  - 808: un seno saturado para que se oiga en parlantes chicos, largo y con glide.
  - ÁCIDO: el de la 303.
  - PULSO: una cuadrada al frente, hueco.
- **Melodías.** Todas llevan saturación antes del filtro.
  - ÁCIDO: la 303 que se abre.
  - FM: un chillido con ruido y modulador realimentado.
  - STAB: dos sierras con ruido por un pasa-banda resonante.
  - SYNC: un sync duro cuya esclava sube con la envolvente.
  - PLUCK: sierras con ruido que se cierran en 70 ms.
  - QUINTAS: sierra más quinta justa, el riff de guitarra.
- **Oscilador.** Pasa por el mismo filtro con LFO libre y por el compresor propio del v2.
  - GRUÑIDO: sierras más una FM de razón 1, cuyo índice respira con el LFO.
  - SIERRAS.
  - FM: con realimentación.
  - CUADRADAS.
- **Kits.**
  - INDUSTRIAL: el del v1, con más pegada.
  - PSY: bombo corto y apretado, con zap.
  - DURO: bombo duro sin rumble, martillo de dos parciales enteras y pistón.
  - ROCK: bombo de parche, caja grande, aro y platillo.
  - TRAP: bombo seco, caja con clap y aro.

**La distorsión es por voz, moderada y siempre antes del filtro.** El rumble saturado del primer
ejemplo industrial se rechazó porque ensuciaba todo; agresivo sí, pero que cada sonido se entienda.

## Samples (dentro del firmware)

Los samples van **en la flash del ESP32**, escritos **dentro de `matriz_oscura.ino`** (un índice cerca del
principio y los datos al final, entre las marcas `// >>> SAMPLES` y `// <<< SAMPLES`): el firmware es un solo
archivo. No hace falta
microSD ni PSRAM. Son **24 frases de un noticiero del futuro**: noticias de una sociedad rota, leídas por
un presentador (Daniel) y una presentadora (Matilda) de ElevenLabs. Cada frase trae el glitch horneado, así
que al ESP32 no le cuesta nada: sílabas que tartamudean, cortes de señal, parlante de tele (pasa-banda de
180 Hz a 6 kHz) y una caída de cinta al final. Ocupan 1,57 MB de flash, por eso el firmware usa la
partición **Huge APP**.

Van en **3 bancos, uno por botón**, de 8 frases cada uno:

| Botón | Banco | Qué dice |
|---|---|---|
| BTN1 | TITULARES | «Última hora: el agua potable ya cotiza en bolsa.» · «Se agota el cielo azul en tres regiones.» · «Gobierno confirma: dormir ahora requiere suscripción.»… |
| BTN2 | PRESENTADOR | «Buenas noches. Esto es Noticias dos mil noventa y nueve.» · «Mantenga la calma. Su emoción ha sido registrada.» · «Gracias por su obediencia.»… |
| BTN3 | SEÑAL CAÍDA (la más rota) | «Señal perdida.» · «Reiniciando la realidad.» · «Los datos mienten.» · «Su humanidad ha expirado.»… |

Las 24 frases están en `BANCOS`, dentro de [`generar_noticias.py`](generar_noticias.py). Cada una tiene su
voz y cuánto glitch lleva: 0,35 los titulares, 0,3 el presentador y 0,8–0,95 la señal caída. Para cambiar
una frase o agregar otras:

```
python generar_noticias.py        # pide a ElevenLabs sólo las frases que faltan en noticias/crudo/ y arma banco/<banco>/N.wav
python preparar_samples.py        # mete banco/ dentro de matriz_oscura.ino (sólo entre las marcas)
```

La clave se toma de `ELEVENLABS_API_KEY` o de `../sampler_ia/secretos.h`, que git ignora. Nunca se imprime
ni se copia. Las voces limpias quedan en `noticias/crudo/` y no se vuelven a pedir, así no se gasta dos
veces: para rehacer una frase se borra su mp3. `--solo-glitch` rehace el glitch sin llamar a la API.

- **Formato:** 44.1 kHz mono, sin bajar la calidad, en **IMA ADPCM de 4 bit**: el codec de
  `sinfonia_natural`, en bloques de 1024. En la voz da 29–34 dB de relación señal/ruido. Si un sample
  quedara bajo 20 dB, el script lo pasa solo a **μ-law de 8 bit**.
- **Preparación:** cada frase va recortada, sin continua y nivelada por RMS a −16 dBFS, medido sólo donde
  suena, con el pico tope en −1 dBFS. El tope de largo es de 5 s; la más larga dura 3,8 s.
- **Los sonidos de antes** (impactos, metales, fx de Freesound, en [`banco/`](banco/)) ya no entran como
  samples. Quedan sólo como fuente de los **recortes de la percusión extra** de Batería. `bajar_samples.py`
  los vuelve a bajar, y los autores están en [`fuentes_samples.txt`](fuentes_samples.txt).

- **BTN1 / BTN2 / BTN3** disparan un sample al azar de su banco, nunca el mismo dos veces seguidas. El que
  sonaba se apaga en 4 ms. No hay reversa: una noticia al revés no se entiende.
- **Pitch:** va de 2 octavas abajo a 1 arriba; al centro hay una zona muerta donde suena como está.
- **Reverb y eco:** los samples mandan a la reverb del golpe final y al eco ping-pong de las melodías.
- **Con la música detenida:** los samples suenan igual, y sus colas se apagan solas.
- **Nivel (`NIVEL_MUESTRA` 1.0):** con 0.55 un sample quedaba unos 10 dB bajo la mezcla; con 1.3 y todo
  al máximo el limitador bajaba 6 dB.

## Matriz

Las 8 escenas del v1 más 3 hipnóticas nuevas, hechas de bandas de color plano con negro entre ellas y
movimiento a saltos en el pulso:

- **INTERFERENCIA:** dos fuentes de ondas que cambian de lugar cada corchea.
- **CUADRADOS:** anillos cuadrados o rombos que nacen del centro, uno por semicorchea.
- **RAYOS:** rayos que giran un escalón por negra y que el bombo ensancha.

Se sortea una escena cada 4 compases, en cada Play y en cada cambio de estilo, nunca la misma. La prueba
de orientación del arranque y el límite de 450 mA son los del v2. Un sample nuevo suelta una figura
grande en la escena FIGURAS.

## Cargar

ESP32S3 Dev Module · **Flash Mode DIO** · USB CDC On Boot Enabled · **Partition Scheme: Huge APP (3MB
No OTA/1MB SPIFFS)**, porque con la partición por defecto los samples no caben. Requiere FastLED.

```
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=dio,PSRAM=opi,PartitionScheme=huge_app" --upload -p COMx .
```

Con los samples adentro el `.ino` pesa ~5.5 MB de texto: la primera compilación tarda unos 5 minutos.

## Verificado (2026-11-01)

**En el PC**, el `.ino` se compiló contra mocks de Arduino/I2S/FastLED y se renderizó:

- **Los 8 estilos con todo sonando**, el oscilador encendido y un sample cada 2 compases: 0 NaN, 0
  muestras en el techo, pico antes del limitador entre 0.66 y 0.81 (el limitador actúa desde 0.89).
- **Los 6 bajos, 6 timbres de melodía, 4 osciladores, 5 kits y 4 patrones en cada estilo:** 0 NaN, 0
  muestras en el techo.
- **Todo al máximo** (todos los volúmenes, filtros abiertos, caídas largas, samples con el pitch en los
  dos extremos): el limitador baja hasta −6 dB y entre 2 y 20 muestras pasan de 0.95; las toma el techo
  suave del final.
- **Disparar un sample no calla nada:** los agudos de la mezcla (los hats, que el sample no tiene) siguen
  iguales durante el sample.
- **Matriz:** las 11 escenas encienden y ninguna pasa de 450 mA.

**En la placa**, con `-DMEDIR=1`: arranca tocando con la densidad al máximo y el oscilador encendido,
recorre los 8 estilos (uno cada 8 s), dispara un sample cada 1,5 s cambiando de banco (y desde 2026-11-02 con la percusión extra y la distorsión del bajo prendidas),
e imprime por Serial0 cada 2 s. Se cargó y se leyó por COM61:

| | CPU promedio | Peor bloque |
|---|---|---|
| matriz_oscura, peor caso | 56–72 % | 72–103 % |

El peor bloque fue en INDUSTRIAL DURO, con la densidad al máximo. Un bloque suelto sobre el 100 % lo
absorbe la cola de DMA de 12 ms; el promedio nunca se acercó. Por eso el silencio que se oyó al disparar
samples en la primera prueba no era CPU. En esa prueba no había archivos en la tarjeta y sonaba el
impacto sintetizado: un sub de 98 → 28 Hz durante 1,6 s, que muchos parlantes chicos compensan bajando
todo. Ahora suenan los samples de la flash, y Gonzalo confirmó que se oyen bien. Además, los dos filtros
de bus se saltan cuando están abiertos del todo, para darle margen al peor bloque.

Lo que ya se recortó para llegar ahí:
- La sub del oscilador (la octava de abajo es del bajo).
- El segundo operador FM: ahora uno sirve a los dos osciladores desafinados.
- Las divisiones de las saturaciones nuevas (`sat3`, cúbica).
- La microSD y su tarea de lectura.
