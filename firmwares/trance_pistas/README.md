# trance_pistas — Trance con peso, sintetizado en 4 pistas, con visuales

El hermano sintetizado de [`mezclador_pistas`](../mezclador_pistas/). Es la misma idea: cuatro pistas que suenan juntas, cada una con volumen, filtro, resonancia y efecto rítmico, y cada una mueve su capa en **Resonancia**. La diferencia es que la música la hace la placa, así que **no necesita el módulo microSD**.

La referencia es **"Hey Boy Hey Girl" de Chemical Brothers**: bombo y bajo pesados, un riff corto que se repite y calza con la armonía, stabs de acordes rave y builds hacia el drop. Se llegó acá tras cuatro rondas rechazadas:
- **Trance de acordes con arpegio:** plano.
- **Modos oscuros con notas de color:** "suena feliz, darkwave".
- **Cyber o industrial:** "le falta trance, le falta peso; las notas que no concuerdan con la armonía no me gustan; el pad se desaprovecha".

Los sonidos de batería y bajo **no se inventaron**: son los de dos firmwares que ya estaban aprobados.

| Pista | Qué suena |
|---|---|
| 0 · batería | **Las voces de [`drum_poder`](../drum_poder/)**, con el kit NEXO, el híbrido 808/909. El bombo tiene tres bandas: fundamental que aterriza en 46 Hz, 2.º armónico con saturación propia, mazo de ruido pasa-banda de 14 ms y click de 5 ms. La caja va afinada (dos parciales que caen un 15 %), con bordonera de ruido y crack de 3,4 kHz, sin saturación. El clap son tres palmadas a 0, 6,5 y 13 ms más la cola. Los hats y el plato son ruido pasa-banda. Cada pista va con **su pasa-altos de banda** (caja 150 Hz, clap 900, hats 2,5 k, plato 1,5 k), la mezcla fija de drum_poder. Dos instancias de bombo y caja: la que sonaba se apaga en 3 ms cuando entra la nueva |
| 1 · bajo | **El de [`bajo_8_pasos`](../bajo_8_pasos/)**: dos sierras PolyBLEP (una +9 cents) y una cuadrada una octava abajo, saturación suave, SVF pasa-bajos con envolvente de filtro de 3,2 octavas que cierra antes que el volumen. Se agacha un 30 % con el bombo para que no se embarren |
| 2 · riff | Un riff corto y sincopado que se repite y **solo toca notas del acorde** del momento. Es un pluck de dos sierras con filtro por nota y eco ping-pong a corchea con punto |
| 3 · acordes | Pad supersaw (2 sierras ±9 cents por nota) que **crece por secciones**: ausente en A, filtrado en A', **se abre durante toda la ruptura** (el build) y en el drop bombea con el bombo (−60 %). Encima van **stabs de acordes rave** y una subida de ruido antes del drop |

## Temas

La armonía es menor natural y consonante (i, iv, v, VI, VII): cero notas que choquen con el acorde. Cada tema es un viaje de **32 compases**:

| Compases | Sección | Qué suena |
|---|---|---|
| 1–8 | A | Batería, bajo y el riff A |
| 9–16 | A' | Entra el pad filtrado y los hats abiertos; en la segunda mitad, los stabs |
| 17–24 | Ruptura | Sin bombo ni bajo. El pad se abre de a poco, el riff B va a media densidad y en los compases 23–24 hay redoble y subida de ruido |
| 25–32 | Drop | Todo: pad bombeando, stabs, riff B y plato |

| # | Tema | BPM | Tónica | Progresión A | Progresión B |
|---|---|---|---|---|---|
| 1 | Fuego | 132 | La menor | i · i · VI · VII | VI · VII · i · i |
| 2 | Pulso | 136 | Mi menor | i · VII · VI · VII | i · VI · iv · v |
| 3 | Motor | 128 | Re menor (big beat) | i · iv · i · iv | VI · iv · VII · i |
| 4 | Trueno | 138 | Fa# menor | i · VI · iv · v | i · VI · VII · VII |
| 5 | Aurora | 140 | Si menor | i · i · iv · iv | VI · VII · iv · v |
| 6 | Vértigo | 134 | Sol menor | i · v · VI · iv | iv · v · i · i |

**BTN1 mantenido 2,5 s** pasa al siguiente tema.

Todo es dato:
- **Batería:** `X` acento, `x` normal, `.` fantasma.
- **Bajo:** `x` tónica, `o` octava, `5` quinta.
- **Riffs (`RIFFS[]`):** índices a las notas del acorde (0–2, y 3–5 una octava arriba).

## Controles

Son los mismos que los de `mezclador_pistas`, con bancos:

- **BTN1** toque = banco general: POT1 bajo · POT2 riff · POT3 batería · POT4 acordes (volúmenes). Mantener 0,5 s = play / stop. Mantener 2,5 s = siguiente tema.
- **BTN2** bajo · **BTN3** riff · **BTN4** batería · **BTN5** acordes. En cada banco: POT1 filtro (centro abierto, izquierda pasa-bajos, derecha pasa-altos) · POT2 resonancia · POT3 efecto rítmico (gate 1/8, 1/16 · repeat 1/2 … 1/16) · POT4 intensidad.
- Al cambiar de banco, una perilla toma el control recién cuando la mueves: nada salta.

## LEDs

- **LED 0:** verde sonando, ámbar en stop; parpadea en blanco con el banco general.
- **LEDs 1 a 4:** cada pista con su color, según su nivel.
- **LED 5:** el pulso. Parpadea en **magenta** si la CPU no alcanza, es decir, si un bloque de audio pasa el 80 % de sus 2,9 ms.

## Resonancia

La línea serial es idéntica a la de `mezclador_pistas` (`R,…` de 43 campos, 60 por segundo), así Resonancia no distingue una placa de la otra. La línea `I` suma un octavo campo con los nombres de las pistas (`batería|bajo|riff|acordes`), y Resonancia los usa en el panel de bancos y en el de capas.

Los golpes no se detectan: salen del secuenciador, así que son exactos.
- **Bajo:** un golpe por grupo de notas.
- **Arpegio:** un golpe por corchea.
- **Pad:** un golpe por cambio de acorde.

## Instalar sin Arduino IDE

Resonancia (`web/public_html/resonancia/`) trae el instalador: **Entrada → Instalar firmware Trance**, con ESP Web Tools. Funciona en Chrome o Edge y necesita https. La imagen está en `resonancia/firmware/trance.bin`. Para regenerarla después de tocar el `.ino`:

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=dio,PSRAM=opi,USBMode=hwcdc" --build-path build trance_pistas
```

Después hay que copiar `build/trance_pistas.ino.merged.bin` a `resonancia/firmware/trance.bin`. Conviene quitarle antes el relleno de `0xFF` del final, redondeado a 4 KB: así baja de 4 MB a unos 480 KB.

Si el instalador no encuentra el puerto: desconecta el cable, mantén **BTN3** (es el GPIO 0, BOOT) y vuelve a conectarlo.

## Verificación

Antes de flashear, el `.ino` se compiló en el PC contra imitaciones de `Arduino.h`, `i2s_std.h`, `FastLED.h` y `Wire.h`, y se renderizó a WAV:
- **Los 6 temas completos:** pico 0,62–0,71, RMS −15 dBFS, 0 recortes, 0 NaN.
- **Cada pista sola y todas al 100 %:** pico 0,91 con el limitador.
- **Filtros a fondo con resonancia máxima, y barridos:** sin NaN.
- **Los 6 efectos rítmicos en todas las pistas, stop/play ×10 y volúmenes 0↔1 de golpe ×40:** 0 recortes.
- **Sin PSRAM:** suena igual; solo se apagan los repeat.
- **Espectro:** ningún pico tonal más de 14 dB sobre su tercio de octava entre 600 Hz y 16 kHz. Arriba de 16 kHz queda 52 dB abajo.

**Y lo que la simulación NO veía:** la primera versión calculaba todo muestra a muestra, recorriendo ~15 voces en cada muestra, con divisiones y `exp2f` adentro.
- **Medido en la placa con el contador de ciclos:** 7.400 ciclos por muestra, de los 5.442 que hay a 240 MHz; el 136 % de la CPU.
- **Por voz:** arpegio 2.330, pad hasta 1.660, batería 870, bajo 500, cadena 2.200.
- **Lo que se escuchaba:** el DMA se quedaba sin datos y metía silencio. Eran cortes como clipeos, el tempo arrastrado y todo "desafinado".

El motor actual va **por bloques**:
- Cada voz se calcula de corrido para las 128 muestras, con su estado en variables locales.
- No hay divisiones por muestra: el PolyBLEP usa `1/inc` precalculado y la saturación es cúbica.
- Los coeficientes de los filtros se recalculan cada 16 muestras, con `exp2Rapido` y `tanRapido`.
- Lo que corre 44.100 veces por segundo va en `IRAM_ATTR`.
- El eco está en RAM interna.

Medido en la placa así: **46 % de CPU en promedio, 59 % de máximo**. Con los 4 repeat activos, filtros y resonancia al máximo: **56 % y 66 %**. 344 bloques por segundo, ningún bloque sobre el 80 %.

Con la batería de drum_poder, el bajo de bajo_8_pasos, el pad y los stabs, medido **en el drop** con los 4 repeat y la resonancia al máximo: **60 % de promedio y 75 % de máximo**, 0 bloques atrasados. Con los efectos apagados queda ~15 puntos más abajo. Para llegar ahí:
- Las voces de bombo y caja se liberan a −70 dB, no a −80; si no, seguían calculando la cola casi 3 s después de dejar de oírse.
- Las capas de ruido se saltan cuando ya son inaudibles.
- El pad usa 2 sierras por nota.

La lección: **el PC no mide el costo en el S3**. Antes de dar por bueno un firmware con varias voces, mídelo en la placa con `ESP.getCycleCount()`.

Requiere **FastLED**. La PSRAM es opcional.
