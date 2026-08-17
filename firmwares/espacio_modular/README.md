# espacio_modular — Ambientes de película

Máquina de ambientes cinematográficos. **Una sola voz monofónica** sobre un dron continuo, con el espacio como protagonista. Un solo panel: 5 botones y 4 pots, cada uno con una función y nada más. No hay combos, no hay paneles ocultos, no hay pots que cambien de significado ni que se congelen.

## Los 24 temas son temas, no figuras

Ésa es la diferencia entre música de película y un arpegio. Un tema se define tanto por su **ritmo largo-corto** como por sus notas, y usa intervalos que significan algo:

- la subida **1 → 5 → 8** (la llamada de trompa, el gesto heroico)
- el descenso **8 → 7 → 6 → 5** (el lamento)
- el suspiro **6 → 5** (la apoyatura que duele)
- el **pedal**: una nota que insiste mientras una sombra entra por debajo

Y sobre todo: **cada nota dura hasta la siguiente**. Los ceros del patrón no son silencios, son la continuación de la nota anterior. De ahí salen las notas de cuatro tiempos y las de medio — 21 de los 24 temas tienen duraciones que van de ×2 a ×7 entre su nota más corta y la más larga. Eso es lo que convierte una lista de alturas en un tema.

| Grupo | Temas |
|---|---|
| **Llamadas** — notas largas, intervalos abiertos | LLAMADA · JURAMENTO · ESTANDARTE · CUMBRE |
| **Lamentos** — el descenso, la nota que duele | LAMENTO · SUSPIRO · CAÍDA · DUELO |
| **Pedales** — una nota que insiste | VIGILIA · SOMBRA · PLEGARIA · ORÁCULO |
| **Cimientos** — con el golpe grave | CIMIENTO · TITÁN · ABISMO · RUINA |
| **Arcos** — pregunta y respuesta | ARCO · TRAVESÍA · REGRESO · PROMESA |
| **Movidos** — cuando la escena avanza | CABALGATA · TORMENTA · RITUAL · ASEDIO |

Los temas pisan a propósito los grados 2, 4, 6 y 7 además de los del acorde: la nota que define un modo casi nunca es del acorde — la 2ª bemol del frigio, el #4 del lidio, el 7 bemol del mixolidio, el si natural de la doble armónica. Si un tema solo tocara 1-3-5-8, los ocho modos sonarían igual.

## Por qué suena a ambiente y no a canción

**Un dron continuo.** Tres osciladores fijos sobre la tónica (fundamental + quinta + octava abajo) que no paran nunca y pasan por el mismo filtro. Es el pedal sobre el que se apoya una escena. Como ya está, los temas no necesitan bajo propio y la voz queda libre para cantar.

**Armonía muy lenta.** La progresión tiene 4 tramos y **cada tramo dura un ciclo entero de 4 compases**: de 9 a 38 segundos por acorde según el tempo, y la vuelta completa de 35 s a 2,5 minutos. Un acorde por compás es ritmo armónico de pop.

**Progresiones no funcionales.** Nada de `i–VI–III–VII` (el bucle pop por excelencia) ni de cadencias `V–i`. La tónica ocupa la mitad del ciclo y lo que ocurre es un **desplazamiento modal**: una sombra que entra y se va.

## Los 8 modos

La paleta es toda cinematográfica: oscura, épica, fantástica, nórdica y árabe. Cada tramo de la progresión dura un ciclo; los puntos indican esa duración.

| # | Modo | Carácter | Progresión | Alturas con tónica en C |
|---|---|---|---|---|
| 0 | **EÓLICO** | épico melancólico | i … VI … i | C D D# F G G# A# |
| 1 | **FRIGIO** | oscuro, amenazante | i … ♭II … i | C **C#** D# F G G# A# |
| 2 | **NÓRDICO** (dórico) | folk vikingo/celta | i … ♭VII … IV | C D D# F G **A** A# |
| 3 | **MIXOLIDIO** | heroico, de aventura | I … IV … ♭VII | C D **E** F G A **A#** |
| 4 | **LIDIO** | fantástico, de maravilla | I … II … I | C D E **F#** G A B |
| 5 | **ÁRABE** (frigio dominante / hijaz) | desierto, exótico | I … ♭II … I | C **C#** **E** F G G# A# |
| 6 | **BIZANTINO** (doble armónica) | exótico oscuro | I … iv … I | C C# E F G G# **B** |
| 7 | **MENOR ARMÓNICA** | épico dramático | i … iv … i | C D D# F G G# **B** |

Todas las progresiones usan **solo tríadas consonantes** del modo: en escalas como la doble armónica aparecen disminuidas y aumentadas que suenan a error, y están evitadas a propósito.

## Controles

### Los 4 pots — siempre lo mismo, siempre vivos

| Pot | Función | Rango |
|---|---|---|
| POT1 (ADC1) | **Volumen** | curva cuadrática |
| POT2 (ADC2) | **Tempo** | 25 – 110 BPM (paso = corchea) |
| POT3 (ADC8) | **Filtro** | 80 Hz – 8 kHz, exponencial |
| POT4 (ADC10) | **Espacio** | eco ping-pong + reverb |

### Los 5 botones — sin combos, cada uno se oye en el acto

| Botón | Función |
|---|---|
| BTN1 (44) | Play / Stop. Cada Play **sortea el sentimiento**: sale un modo nuevo, con su progresión |
| BTN2 (42) | **Tema al azar** (de 24), manteniendo el sentimiento |
| BTN3 (0) | **Modo en orden** (8), para explorar los vecinos del que te gustó |
| BTN4 (45) | **Tonalidad**: sube la tónica una cuarta justa (ciclo de cuartas) |
| BTN5 (47) | **Octava** base (-2 → -1 → 0 → +1 respecto a C3) |

Ni BTN1 ni BTN2 repiten nunca lo que acababa de sonar. La semilla del azar sale del ruido de los bits bajos del ADC, y cada sorteo mezcla `micros()` — el instante exacto de la pulsación es la mejor entropía que hay en una placa sin reloj, así que dos encendidos no dan la misma secuencia.

### IMU — un solo eje

La aceleración en **X** multiplica el corte del filtro hasta **×8** (tres octavas) por encima de donde tengas el POT3. Inclinar la placa = barrido. El eje Y no se lee.

### El timbre es fijo

No se toca desde los controles: onda **diente de sierra**, todas las capas encendidas (sub + quinta + octava), dron a nivel medio, envolvente de pad y resonancia media. Si alguna vez quieres cambiarlo, están todos juntos en el bloque `TIMBRE FIJO` del `.ino`, cada uno en una línea.

### Saber qué está sonando

Por **Serial a 115200** sale una línea cada vez que aprietas un botón:

```
MODO NORDICO      TEMA LAMENTO      TONICA F   OCTAVA +0  BPM 63
```

Es para que cuando algo te guste sepas qué era. Se imprime solo al pulsar, nunca dentro del audio. Se apaga poniendo `MOSTRAR_ESTADO` en 0.

## Motor de sonido

- **Banco de 6 osciladores** sobre esa única nota: principal + 2 gemelos desafinados + sub (-12) + quinta (+7) + octava (+12), con portamento y paneo por oscilador.
- Sierra con anti-aliasing PolyBLEP.
- **Filtro biquad resonante estéreo** (corte ligeramente distinto en L y R → imagen ancha).
- **Eco ping-pong** + **reverb** de 6 combs y 4 all-pass.
- Soft-clip tipo tanh a la salida.

## Higiene de audio

Cinco cosas que conviene no volver a romper:

- **El largo del paso se fija al empezar cada paso.** Si el tempo puede acortar el paso *en curso*, subirlo dispara una nota retroactivamente: al barrer la perilla, decenas de notas atropelladas. El tempo nuevo entra en el paso siguiente.
- **El tiempo del eco es fijo, no sincronizado al tempo.** Sincronizarlo obliga a mover el puntero de lectura con cada cambio de BPM, y eso se oye como un barrido de altura mientras giras la perilla.
- **El eco satura suave dentro del lazo.** Recortar duro la realimentación deja recirculando la distorsión.
- **El paneo está suavizado.** Aplicar `stepPan` directo hace saltar las ganancias L/R en medio de una nota con la envolvente arriba: eso es un chasquido.
- **Ganancia normalizada** por la suma de niveles de los osciladores y por `Q^0.30`.

## Verificado en simulación

| Prueba | Resultado |
|---|---|
| Barrido del pot de tempo con ruido de ADC | 0 notas atropelladas · 0 saltos > 3000 |
| El tempo manda en el ritmo | 2 → 11 notas en 6 s (25 → 110 BPM) |
| Los 5 botones producen sonido | dentro del primer buffer (2.9 ms) |
| Notas en tono | **0 fuera de 5376** (24 temas × 8 modos × 4 tramos) |
| Ritmo largo-corto | 21 de 24 temas con duraciones de ×2 a ×7 |
| Tríadas de las progresiones | 0 disminuidas, 0 aumentadas |
| Chasquidos / clipping / NaN | 0 |

## Compilar

Arduino IDE, board **ESP32S3 Dev Module**:

- USB CDC On Boot: **Enabled**
- Flash Mode: **DIO** (¡OPI rompe el I2S!)
- PSRAM: **OPI PSRAM**

Sin librerías externas (solo `Wire.h` del core). Ocupa 29 % de flash y 59 % de RAM (los buffers de eco y reverb son estáticos).
