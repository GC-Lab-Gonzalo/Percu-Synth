# drum_poder

Drum machine 100 % sintetizada (sin samples). **Es percusión y nada más**: no toca una sola nota, ni por el parlante ni por el MIDI. Por el DIN-5 sale el **reloj**, y la melodía la pone el sinte que lo recibe.

Dos reglas mandan sobre todo lo demás, y de ellas sale el sonido:

**1. La percusión no hace melodía. Ninguna.** El **SUB** suena la **tónica** del groove y no se mueve nunca. El bombo, los toms y el cuerpo de la caja son percusión afinada con *su* altura fija, y todo lo que vive arriba de 1 kHz (hats, metal, clap, FX) es **ruido filtrado**: no tiene nota, así que no puede hacer melodía. Verificado recorriendo las 12 progresiones: **ninguna voz cambia de altura**.

**2. Arriba de los graves todo es corto.** El presupuesto de decay va por banda:

| Banda | Tope de tau | Medido a −40 dB del pico |
|---|---|---|
| abajo de 100 Hz (sub, bombo) | 0.70 s | sub **2.1 s** · bombo **1.3 s** |
| 100–250 Hz (toms) | 0.22 s | tom **0.7 s** |
| 250 Hz–1 kHz (cuerpo de caja) | 0.10 s | caja **0.5 s** |
| arriba de 1 kHz | 0.14 s (el plato, 0.50) | clap **0.25 s** · hat **0.13 s** · plato **1.9 s** · FX **1.0 s** |

El peso abajo y la definición arriba salen de ahí. Las 8 pistas, cada una en su banda:

| Pista | Banda | Qué es |
|---|---|---|
| **SUB** | 33–45 Hz de fundamental **+ 2º y 3er armónico** (sin ellos no se oye en un parlante chico), la tónica fija | el peso. Es la voz larga |
| **BOMBO** | 39–54 Hz de fundamental + **pecho en 80–110 Hz** + mazo en 0.9–1.9 kHz | el ancla |
| **TOMS** | 88–225 Hz, pasa-altos en 70 | el medio-grave |
| **CAJA** | 175–345 Hz + 1.9–3 kHz de bordonera | pasa-altos en 150 |
| **CLAP** | 0.9–3.5 kHz de ruido | el snap que dobla la caja |
| **FX** | barridos de ruido de 1.3 a 7 kHz | caída y riser: son **transiciones**, un barrido por frase, no un pulso |
| **HATS** | 6–8 kHz de ruido pasa-**banda** | aire, nunca "shhh" |
| **METAL** | 3.6–5.2 kHz de ruido pasa-**banda** | el plato. Cero parciales afinados, y en los grooves marca el "1" y los giros, nunca un pulso |

Los ritmos **no son aleatorios**: son **12 grooves escritos a mano** con compases impares de verdad (7/8, 5/8+7/8, 9/8, 13/8, 5/4, 12/8), con el **aire como parte del groove**. Y por el **MIDI DIN-5** (GPIO 43) sale el **reloj** para que el sinte externo corra enganchado.

### El bombo: tres bandas

Un bombo sintetizado pesa cuando tiene las tres, y la que más se olvida es la del medio:

| Banda | Qué aporta | Cómo se hace |
|---|---|---|
| **39–54 Hz** | el peso | seno que cae desde 4.6–7× en 16–42 ms |
| **80–110 Hz** | la **pegada en el pecho** | el 2º armónico con nivel de verdad (`kPunch` 0.28–0.50). Es la banda que hace que se sienta grande en un parlante que no da 40 Hz; sin ella el bombo suena a "boom" flojo y lejano |
| **0.9–1.9 kHz** | la definición | el mazo: una banda de **ruido** de 14 ms con su propia envolvente |

Más su **saturación propia** (1.40–1.75), que acá sí va: el bombo tiene una sola parcial fuerte, así que saturarlo le genera armónicos en vez de intermodulación sucia. Es otra parte de por qué se oye en chico.

> **El mazo va con ruido, no con un seno.** Un seno corto en 1–2 kHz metido dentro del golpe es literalmente un **pitido pegado a cada bombo**, y es el error que tuvo este firmware durante tres versiones. Un mazo de verdad no es un tono: es madera contra un parche, o sea una banda de ruido. Da la misma definición y no canta ninguna nota.

Y para que el grave pese, además del bombo:

- **El SUB va en el mismo bus que el bombo**, y ese bus **no se agacha a sí mismo**. Son un solo instrumento: si el sidechain agachara al sub justo cuando suenan juntos — que es en cada apoyo — se estaría comiendo el peso que el sub existe para dar.
- **Un realce fijo de +3.5 dB en 85 Hz** en el master, antes del saturador (así el peso que se agrega también engorda con él). Es la única EQ del bus, está clavada y no tiene pot: un shelf de 3.5 dB no tiene pico ni resonancia, así que no puede sonar mal en ninguna posición.
- Y si querés más, el **POT3 (CUERPO)** sube la saturación de la banda grave hasta ×3.6.

### Los FX

En el lugar donde antes había un tick de baqueta — que se oía como **percusión latina**, que no es lo que hace esta máquina — hay ahora barridos de ruido: una **caída** (pasa-banda de agudo a medio) y un **riser** (al revés, con swell). Son **transiciones**: uno por frase, en los puntos de giro, nunca un pulso. Q bajo a propósito, porque un pasa-banda de ruido con Q alto barriendo se oye como un silbido de pájaro y no como aire.

## Controles

**Los mismos cinco botones y los mismos cuatro pots de [`drum_ruido`](../drum_ruido/), uno por uno.** Cada botón hace una sola cosa, en el **flanco de presión**; cada pot significa siempre lo mismo y la posición física de la perilla **es** el valor. No hay paneles, no hay combos y no hay pots congelados: todo eso existió mientras el firmware generaba las notas del bajo, y se fue con el bajo.

| Control | Acción |
|---|---|
| **BTN1** (44) | **GROOVE**: siguiente de los 12 ritmos. Cambia compás y tempo sugerido, reubica el "1" y manda **Stop+Start** por MIDI para que el sinte arranque su secuencia en ese mismo "1". |
| **BTN2** (42) | **KIT**: cicla los 5 timbres. Cambia la síntesis al instante; el groove sigue. |
| **BTN3** (0) | **TAP TEMPO**: marca el pulso (negras). Con 2 toques fija el BPM (50–220). El primer toque tras 2 s de silencio reubica el "1" y realinea el sinte. |
| **BTN4** (45) | **MEDIO TIEMPO** (mantener): cada paso del patrón dura el doble. Al soltar retoma **sin desfase**. |
| **BTN5** (47) | **REDOBLE** (mantener): fill de toms y caja que acelera; al soltar, platillo. Bombo y sub siguen debajo. |

| Pot | Acción |
|---|---|
| **POT1** (ADC1) | **VOLUMEN** master. A cero es el mute (la máquina siempre suena, no hay play/stop). |
| **POT2** (ADC2) | **FILTRO**: corte del pasa-bajos global, 200 Hz → abierto. Es el control con el que se oscurece todo. La resonancia **sólo aparece cuando el corte ya bajó de 2.5 kHz**; más arriba el filtro es transparente. Cada golpe abre un poco el corte, así respira con el ritmo. |
| **POT3** (ADC8) | **CUERPO**: saturación en **dos bandas** — graves y medios engordan (hasta ×3.6), los agudos casi no se tocan (×1.2). Suma peso, no suciedad. |
| **POT4** (ADC10) | **BEAT REPEAT**: OFF · ×2 (loop de 8 pasos) · ×4 (4) · ×8 (2) · ×16 (1 paso). |

El **beat repeat** loopea sobre un reloj maestro que nunca se detiene, así que al volver a OFF la máquina retoma en tiempo. Las zonas del pot tienen **histéresis** y el ancla **busca hacia atrás un bloque con golpes**: si engancha un tramo vacío del patrón, el break se convertiría en silencio.

## Los 12 grooves (BTN1)

| # | Groove | Compás | BPM | Tónica del sub | De dónde viene |
|---|---|---|---|---|---|
| 1 | **CADENA** | 4/4 medio tiempo | 76 | Re eólico | El golpe grande: caja en el 3, bombo escaso y empujado, hats de trap en semifusas por encima |
| 2 | **VELO** | 4/4 medio tiempo lento | 66 | Do# eólico | Dos golpes por compás y el resto es sala. Para el kit ABISMO |
| 3 | **OFRENDA** | 4/4 con swing | 92 | Mi dórico | El groove lineal, casi góspel, con la caja hablando bajito entre los acentos |
| 4 | **ESPIRAL** | 7/8 (3+2+2) | 128 | Re frigio | El bombo marca la agrupación y la caja cae siempre en el mismo lugar: por eso el compás no se pierde |
| 5 | **CISMA** | 5/8 + 7/8 | 122 | Re frigio | Cada compás son dos sub-compases de largo distinto; lo sostiene el metal en el arranque de cada uno |
| 6 | **LABERINTO** | 9/8 (3+3+3) | 112 | Si eólico | El bombo dibuja los tres grupos, la caja va corrida contra ellos |
| 7 | **TRECE** | 13/8 (3+3+3+2+2) | 120 | Mi locrio | El compás raro por excelencia, con dos puntos fijos de caja como referencia |
| 8 | **VÓRTICE** | 4/4 doble bombo | 168 | Mi menor armónica | Semicorcheas de bombo debajo de un backbeat firme |
| 9 | **PRISMA** | 5/4 | 144 | Sol dórico | El impar "amable": cinco negras claras, con el metal cada dos |
| 10 | **HÍBRIDO** | 4/4 electrónico | 96 | Re frigio | Hats de trap, clap doblando la caja, tick de shaker y el SUB como 808 en cada apoyo |
| 11 | **CORAL** | 12/8 | 62 | La eólico | El pulso de corchea con puntillo: la balada épica para respirar |
| 12 | **DESPLAZADO** | 4/4 (3+3+3+3+2+2) | 138 | Re frigio | El riff entra y sale del pulso, pero la caja se queda clavada en el 2 y el 4 |

Los patrones se escriben **como texto**, un carácter por semicorchea y un compás por cadena, así que un groove se lee de un vistazo y se edita sin tocar código:

```c
/*BOMBO*/ "X-----x-----x---"  "X-----x---x-x---",
/*CAJA */ "--------X-------"  "--------X-----x.",
/*HATS */ "x.x.x.o.x.x.xRx."  "x.x.R.o.x.x.RRx.",
```

`-` silencio · `.` fantasma · `x` normal · `X` acento, más los caracteres propios de cada pista (`d`/`D` doble bombo, `f` flam, `r`/`R` redobles, `1 2 3`/`L M H`/`l m h` los tres toms, `o`/`O` hat abierto, `c`/`C` plato largo, ). La gramática completa está comentada arriba de la tabla `GROOVES[]`.

La **progresión avanza un grado por compás**, en ciclo de 4. Como varios grooves tienen frases de 2 compases, la armonía tarda dos vueltas en cerrar: eso es lo que hace que un loop de 2 compases no se sienta como un loop de 2 compases. Con `FILL_AUTO` (activado) el último compás de cada 4ª repetición mete un redoble de toms y cierra con platillo.

## Los 5 kits (BTN2)

| Kit | Carácter |
|---|---|
| **RITUAL** (cian) | Bombo redondo y profundo, caja grande y afinada, hats de aire. El más "Sleep Token" de los cinco. |
| **MONOLITO** (violeta) | Seco y orgánico: toms adelante, caja tipo rimshot. Casi sin sala: suena en la habitación, no en una catedral. |
| **PRISMA** (ámbar) | Rápido y apretado: aguanta dobles de semicorchea sin emborronarse porque todo es corto. |
| **NEXO** (verde) | Híbrido electrónico: bombo 808 profundo, sub largo, y todo lo de arriba cortísimo (909). |
| **ABISMO** (rojo) | El más grave y el más grande: el sub llega a 0.70 s de tau y el bombo a 0.28, pero lo de arriba sigue corto. |

Cualquier kit se cruza con cualquier groove. Toda la síntesis vive en la tabla `KITS[5]`: cada fila es un timbre completo (bombo, caja, toms, hats, metal, clap, tick, sub) más su sala.

## El MIDI DIN-5: sale el reloj, no notas

Sale por el **GPIO 43** (el TX del DIN-5 de la placa), **31250 baud**: **MIDI Clock a 24 PPQN** y **Start**. Nada más. Este firmware no toca una sola nota.

**El bajo lo pone el sinte.** Esto tuvo tres versiones con bajo adentro y las tres se descartaron, cada una por una razón distinta y cada una útil:

1. **Una línea escrita a mano por groove.** Doblaba al bombo, o sea que no era una línea de bajo: era el bombo otra vez.
2. **Una tabla de ritmos de bajo independiente** (galope, 3-3-2, funk, contratiempo…) leída sobre una grilla y combinada con células melódicas. Cada patrón *por sí solo* estaba bien, pero **caía donde el groove no tenía nada**: contra un 7/8 o un 13/8 escritos a mano eso no es una variación, es otra máquina tocando encima.
3. **Patrones derivados del bombo del groove.** Calzaban — se verificó que ninguna nota caía fuera — pero seguían siendo aburridos: un bajo generado por reglas suena a bajo generado por reglas.

La conclusión es la buena, y es más simple que las tres: **elegir el sonido y la secuencia en el sinte es infinitamente más expresivo que cualquier tabla metida acá adentro**, y el único trabajo real de esta máquina es dar un pulso que no se mueva.

Dos cosas hacen que ese pulso sirva:

- **El reloj se cuenta con el contador de muestras del audio, pero se manda desde la tarea de control.** Contarlo con el audio es lo que hace imposible que se vaya de fase con la batería (las dos cosas salen del mismo contador, no de dos relojes distintos). Mandarlo desde la tarea de control es lo que le da resolución de **1 ms** en vez de la del buffer de audio: mandarlo desde el render metía **±2.9 ms** de fluctuación en cada pulso, que a 120 BPM es un 14 % del tick y se oye como un arpegio tembloroso.
- **Se manda Stop+Start cada vez que se reubica el "1"** — al cambiar de groove y en el primer tap de una serie de tap tempo. Sin eso el sinte sigue en el paso donde estaba y su secuencia queda corrida contra la batería para siempre. El Stop antes del Start no es adorno: hay equipos que ignoran un Start si creen que ya están corriendo.

Medido en los 12 grooves, con sus tempos y compases distintos: el tempo del reloj cae dentro del **0.04 %** del BPM del groove.

`ENVIAR_MIDI_CLOCK = false` apaga todo el transporte.

> **Este firmware no imprime nada.** En esta placa el UART0 sale por los GPIO 43/44 y el 43 es justo el TX del DIN-5, así que cualquier `Serial0.print` saldría por el cable MIDI como basura (y el RX, el 44, es el BTN1). Pero además un print por USB CDC **bloquea al núcleo que lo hace hasta que el host lea**, y mientras tanto el DMA del DAC se queda sin datos: eso se oye como un chasquido. Toda la información de estado la dan los 6 LEDs.

## La mezcla

La tabla de pistas de arriba **es** la mezcla, y no la cambia el kit: cada pista lleva su ganancia, su panorama, su envío de sala y su **pasa-altos de banda** fijos (`TRACK_GAIN`, `TRACK_PAN`, `TRACK_SEND`, `TRACK_HP`). Los pasa-banda de Q bajo (hats, metal, tick) tienen faldas anchas, así que además llevan pasa-altos de banda en 2.5 kHz / 1.5 kHz: sin eso un hat centrado en 6.8 kHz todavía dejaba energía en 250–900 Hz, o sea encima del cuerpo de la caja.

El **bombo y el sub van con envío 0 a la sala**: mojar el grave es la forma más rápida de perder el golpe. Y la sala está normalizada por su propia realimentación (`rv *= 0.25·(1−fb)`): sin eso crece al subir el `revFb` del kit y en un patrón denso florece hasta comerse el limitador — se oye como si todo se ensuciara de golpe.

Verificado disparando cada golpe **solo** y midiendo la energía media por bin en seis bandas: los 10 golpes × 5 kits caen todos en su banda, y ninguno se mete en el grave por encima de su tope.

## La masterización

Cadena fija, en este orden — y el orden es la mitad del asunto:

**sidechain del bombo → sala corta → saturación de CUERPO en dos bandas (POT3) → FILTRO pasa-bajos resonante (POT2) → compresor de bus (1:2.8 fijo) → pasa-altos de 30 Hz → LIMITADOR con lookahead (techo 0.92) → pasa-bajos de 13 kHz → volumen (POT1) → techo final con rodilla**

- **Toda la distorsión va antes del filtro.** Una no linealidad después reinyecta agudos que el filtro ya no puede sacar, y el POT2 dejaría de poder oscurecer del todo.
- **El pasa-bajos de 13 kHz va después del limitador.** Un limitador multiplica por una ganancia que se mueve, y multiplicar es modular: genera bandas laterales. Si el filtro va antes, esa basura ya no tiene quién la filtre.
- **El limitador mira 64 muestras hacia adelante** (1.45 ms). Sin lookahead siempre llega tarde al golpe — por rápido que sea su ataque, el pico ya pasó — y el que termina agarrándolo es el clipper del final: distorsión en cada golpe fuerte. Con lookahead el techo se respeta exacto: medido, el pico de salida es exactamente 0.92 × volumen, con 0 muestras al tope.
- **El techo final es lineal hasta 0.95** y sólo tiene rodilla arriba de eso. Un `softClip` clásico distorsiona un poco en todo su rango, y esos armónicos nacen después del pasa-bajos, así que ya no hay nada que los filtre.
- **Sidechain**: cada bombo agacha el resto 2.5 dB durante ~90 ms. Es lo que hace que el bombo *se sienta* y lo que le abre hueco al sub.

## Los dos núcleos (y el chasquido)

El audio corre en **su propia tarea, clavada al core 1**; los botones, los pots y los LEDs corren en **otra tarea en el core 0** a 1 kHz. La tarea de control **no toca el secuenciador ni las voces**: deja un pedido (`reqGroove`, `reqBPM`, `reqRepeatLen`…) y la tarea de audio lo aplica en el borde de un buffer. Sin eso habría carrera sobre `masterStep` y sobre el pool de voces, y una carrera ahí se oye como un golpe partido.

Esto es la corrección de un chasquido intermitente que aparecía **al final de un patrón, en los grooves rápidos y en los fills**. No era la envolvente ni el robo de voz: medido en simulación sobre los 12 grooves, **con el redoble sostenido**, hay 0 robos y 0 cortes secos. Era el presupuesto de tiempo. El render tiene que entregar 128 muestras cada **2.9 ms** y la cola del DMA son 4 descriptores (~12 ms); todo eso vivía en el mismo `loop()` que las lecturas del ADC y `FastLED.show()`. Cuando el patrón es denso (OFRENDA tiene ~25 golpes por compás) o hay un redoble, la vuelta se pasa del presupuesto, el DMA se queda sin datos y el driver — que está en `auto_clear` — saca **ceros**. Eso es el chasquido.

Del mismo trabajo salieron tres cosas más:

- **Fuera todo el `Serial`.** Un print por USB CDC bloquea al núcleo que lo hace hasta que el host lea. En un firmware que va justo de tiempo eso es exactamente el mismo problema.
- **`readPot` toma 4 muestras, no 8.** Cada `analogRead` del S3 cuesta decenas de microsegundos.
- **El robo de voz no tiene atajo por `dying`.** Una versión intermedia prefería robar una voz que ya se estaba apagando; el problema es que una voz recién *fast-killeada* todavía está a amplitud casi plena (el fundido es de 3 ms), así que pisarla era el clic que se quería evitar. El criterio de energía ya la elige sola cuando de verdad se apagó.

## Las reglas de calidad (no romperlas al editar)

Cada una salió de una **medición**, no de una corazonada. Las primeras dos son las que definen qué es este firmware:

- **Arriba de los graves nada es melódico.** Sólo el SUB sigue la armonía. Verificado recorriendo las 12 progresiones: la única voz que cambia de altura es el SUB, y ninguna voz con altura definida arranca arriba de 260 Hz.
- **Arriba de los graves todo es corto**, con el presupuesto por banda de la tabla de arriba. Verificado disparando cada golpe solo y midiendo cuánto tarda en caer 40 dB.
- **Tres envolventes por golpe**: el mazo del bombo tiene que morir en 20 ms mientras el cuerpo grave sigue 300, y el crack de la caja igual. Con envolventes compartidas es imposible.
- **Un parche por instrumento.** Bombo, caja, cada tom, hi-hat, cada zona del metal y el sub son **monofónicos**: un golpe nuevo apaga el anterior con un fundido de 3 ms (el *choke* del hi-hat es exactamente esto). El clap y el tick quedan fuera: las réplicas agendadas *son* el clap.
- **Sólo se satura lo que tiene UNA parcial fuerte** (bombo y sub, nada más). La caja tiene dos parciales de cuerpo y el tom tiene fundamental + parcial de parche: saturarlos genera sumas y diferencias entre ellas, y eso deja un **tono puro** dentro del golpe. Medido: la caja tenía un tono en 636 Hz a 16 dB sobre sus vecinos y el tom uno en 899 Hz a 13 dB — dos pitidos más, encontrados por el detector.
- **Ningún golpe de percusión puede tener un tono puro arriba de 600 Hz.** Se verifica por la *forma* del espectro: una senoidal sobresale más de 12 dB sobre sus vecinos a un tercio de octava, el ruido filtrado no. Medido ahora: el peor caso de todo el kit es 7 dB (el parcial de parche del tom, que es un armónico legítimo de un tambor afinado) y el bombo está en 2.7 dB.
- **Los hats y el metal van con pasa-BANDA, no con pasa-altos.** Un pasa-altos deja pasar todo hasta Nyquist y esa octava de arriba no aporta más que filo. Con Q bajo, además, hace falta el pasa-altos de banda para cortarles la falda de abajo.
- **La resonancia del filtro depende del corte, no del pot.** Si sube apenas se baja el pot, con esta curva a 3/4 de recorrido el corte está en 10 kHz: un pico resonante justo en la zona que molesta. Acá sólo entra cuando el corte bajó de 2.5 kHz.
- **El tope del filtro va en 15 kHz, no en 19.** Un biquad RBJ tan cerca de Nyquist deja de ser transparente y hunde la banda de 8–16 kHz.
- **Los golpes del redoble salen cortos** (decay ×0.40) y nunca más rápido que 30 ms entre golpe y golpe: un roll con los golpes largos del kit es barro.
- **El umbral de apagado de la voz importa.** A −64 dB una cola de 1.5 s reserva la voz 11 s después de dejar de oírse y la polifonía se agota tocando normal; a −48 dB se libera 3 veces antes y el corte no se escucha.
- **Cero robos de voz.** Un robo es un golpe cortado en seco, o sea un clic. El firmware los cuenta y el simulador exige que sean 0 — incluso con el redoble sostenido en los 12 grooves.
- **Ninguna voz se libera cortando la onda.** Al cruzar el umbral de release la envolvente pasa a una tau de 1.5 ms y la voz se suelta recién en −82 dB. Soltarla en el umbral (−48 dB) deja un escalón, y un escalón es un clic de banda ancha: medido, eran ~100 por segundo.
- **Nada de `Serial`.** Un print por USB CDC bloquea al núcleo que lo hace hasta que el host lea, y mientras tanto el DMA del DAC se queda sin datos.
- **El largo del paso se latchea al empezar el paso** (por el swing y el medio tiempo). Si puede cambiar a mitad de camino, mover el tempo retro-dispara la nota que ya sonaba.

## Verificación por simulación

El `.ino` se compila en el PC contra mocks de `Arduino.h` / `driver/i2s_std.h` / `FastLED.h` y se mide lo que sale del DAC y del puerto MIDI:

- **Tabla de grooves**: los 12 × 8 líneas con el largo exacto y sin un carácter fuera del alfabeto de su pista.
- **Largo de cada golpe** (a −40 dB, disparado solo): los 10 golpes × 5 kits dentro del presupuesto de su banda.
- **Nada melódico**: recorridos los 12 grooves, ninguna voz cambia de altura.
- **Audio**, 12 grooves × 5 kits: **0 NaN**, **0 muestras al tope**, factor de cresta **4.0–9.4** (dinámica de verdad, no una pared aplastada), y nunca se agotan las voces.
- **Peor caso** (redoble sostenido en los 12 grooves, volumen y cuerpo al máximo): **0 robos de voz**, **0 cortes secos de envolvente**, 0 muestras al tope.
- **MIDI**: sólo salen Clock, Start y Stop — ni un byte más. El tempo del reloj cae dentro del **0.04 %** del BPM del groove en los 12, y se manda un Stop+Start exactamente donde se reubica el "1".
- **Mezcla**: cada golpe en su banda, ninguno metiéndose en el grave.
- **Brillo**: la banda de 8–20 kHz queda **40 dB** bajo la dominante (antes eran 20).
- **Lo que sobrevive arriba del techo de 13 kHz**: −50 dB respecto del grave con el volumen y la pegada al máximo.
- **El filtro**, sobre un golpe aislado y con el ruido sembrado igual: oscurece de forma monótona en todo el recorrido.
- **Pitos**: 9 golpes × 5 kits, midiendo la *prominencia* de cada bin sobre sus vecinos a un tercio de octava. Ninguno pasa de 12 dB, o sea que no hay un solo tono puro escondido arriba de 600 Hz.
- **Medio tiempo · beat repeat · botones · pots**: como se describe más arriba, todo verificado por el flanco de presión.

El detector de pitos **promedia 8 golpes con semillas de ruido distintas**, y eso es indispensable: el espectro de una ráfaga de ruido de 20 ms es una realización aleatoria y sus bins fluctúan ±10 dB, así que medir un golpe solo inventa picos que no existen. Un tono determinista sobrevive al promedio; el ruido se aplana.

Dos avisos sobre el propio medidor: hay que **promediar varias ventanas** de análisis (con una sola de 186 ms el resultado depende de qué golpe cayó dentro) y hay que **sembrar los dos generadores aleatorios** (humanización y ruido de audio), porque un hat es un golpe de ruido de 30 ms y su espectro es una realización que se mueve varios dB. Y una advertencia de método: medir el filtro sobre el groove completo **no es válido**, porque el compresor y el limitador redistribuyen ganancia entre el momento del bombo y el de los hats.

Compilado y verificado con `arduino-cli` para `ESP32S3 Dev Module` (DIO · OPI PSRAM · 240 MHz): **408.922 bytes de programa (31 %)** y **56.372 bytes de RAM (17 %)**, sin advertencias propias.

## Hardware

- ESP32-S3 + PCM5102 (I2S, estéreo 44.1 kHz / 16-bit).
- Salida **MIDI DIN-5** por el GPIO 43 (31250 baud).
- 6 LEDs WS2812 SMD internos de la placa (pin 46) — **sólo indicadores**: color = kit, LED0 bombo, LED1 caja/clap, LED2 hats/metal, LED3 toms, LED4 tick/sub, LED5 pulso; N LEDs marcan el groove/kit/modo de bajo elegido, LED5 amarillo = medio tiempo, blanco = redoble.
- **No usa IMU**: todo el CPU va al audio y al MIDI.

## Arduino IDE

Board **ESP32S3 Dev Module** · USB CDC On Boot **Enabled** · **Flash Mode DIO** (¡OPI rompe el I2S!) · PSRAM **OPI PSRAM** · Partition **Default 4MB with spiffs** · CPU **240 MHz**.
Librería extra: **FastLED** (gestor de librerías de Arduino).
