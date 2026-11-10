# matriz_gotica

Hermano de [`matriz_oscura`](../matriz_oscura/) con **un solo estilo** y **6 escenarios**. La misma banda
en la placa (batería, bajo, melodías y oscilador, cada uno con su panel, más el panel de samples) y la
misma lógica: las secuencias corren siempre y todo se escribe en grados de la escala.

Lo que cambia:

- **El estilo es uno**: el cruce del INDUSTRIAL DURO de matriz_oscura (martillos y pistones en 3+3+2, la
  nota de ruido que insiste con la b2) con su PSYTRANCE (bombo-bajo rodando, el ácido que se abre),
  **producido como un disco gótico**.
- **El BTN2 del General ya no cambia de estilo: cambia de ESCENARIO.** Todo el firmware vive en una sola
  tónica (**Mi**): Mi frigio (E F G A B C D) salvo BAILE y SALÓN, que van en Mi dórico (el cuarto acorde
  mayor). Cada escenario es otro lugar de la misma
  canción: otros patrones, otros timbres, otra progresión dentro de la escala, otro espacio. La escala, la
  tónica y el tempo no se mueven.

Es **un solo archivo**: el `.ino` trae adentro el noticiero glitcheado del panel de Samples (el mismo de
matriz_oscura). Se comparte ese archivo y se carga tal cual.

## Escenarios (BTN2 del General)

| # | Escenario | Progresión | Qué pasa | Kit · bajo · melodía | Colchón | Arpegio |
|---|---|---|---|---|---|---|
| 0 | **CRIPTA** | Em fijo | El industrial duro: martillos y pistones, bajo sucio entre los bombos. Seco, encerrado | DURO · SUCIO · STAB | — | pluck |
| 1 | **TÚNEL** | Em ×7 · F | El psytrance: bombo-bajo rodando, el ácido con la b2 | PSY · RODANTE · ÁCIDO | — | pluck |
| 2 | **BAILE** | Em7 ×2 · A7 ×2 (dórico) | Pop bailable de los 80: caja enorme con puerta, bajo funk sincopado, guitarra rítmica en el contratiempo | OCHENTA · FUNK · GUITARRA | — | rasgueo |
| 3 | **PROCESIÓN** | Em ×2 · Dm · C | Rock gótico: bajo en corcheas, caja en 2 y 4, riff y arpegio de guitarra | GÓTICO · GÓTICO · GUITARRA | — | guitarra |
| 4 | **FORJA** | Em ×6 · F ×2 | La cumbre: los martillos del duro + el bajo rodante del psy + el coro | DURO · RODANTE · STAB | coro | pluck |
| 5 | **SALÓN** | Em ×2 · G · A (dórico) | El otro de baile: los acordes suben de vuelta a la tónica, con el coro atrás | OCHENTA · FUNK · GUITARRA | coro | rasgueo |

El cambio es inmediato, en el flanco del botón. En CRIPTA y TÚNEL el patrón 1 y los timbres son los
aprobados en matriz_oscura, llevados a Mi frigio. Tempo: lo pone el POT2 (va bien entre 125 y 140).

BAILE y SALÓN salieron de escuchar: los escenarios CLAUSTRO (clavecín y cuerdas) y NAVE (medio tiempo) no
gustaron, y se pidió «algo como Let's Dance de Bowie». De ahí se tomó el lenguaje, no la canción: el kit
**OCHENTA** (la caja con el ruido casi plano que una puerta corta a los 170 ms), el bajo **FUNK** y el
**rasgueo** (el arpegio toca el acorde entero, cortito, donde el patrón tiene una nota).

## Lo gótico

- **Guitarra con chorus** (timbre GUITARRA de las melodías y arpegio de guitarra): dos sierras apenas
  desafinadas, casi sin saturar, el filtro se cierra lento y la nota queda sonando; pasa por el chorus
  (casi todo mojado) y por el eco a corchea con puntillo.
- **Bajo GÓTICO**: dos sierras a 12 cents (ese batido lento es el chorus) y un seno que deja quieta la
  fundamental. Va una octava arriba y camina por el acorde.
- **Kit GÓTICO**: bombo de caja de ritmos, caja grande, dos toms (alto y de piso) para los ritmos
  tribales, platillo.
- **Coro oscuro** y **cuerdas frías**: un colchón de tres notas con el acorde. El coro son dos formantes
  fijos con vibrato; las cuerdas, un pasa-bajos una octava arriba. Al cambiar de acorde se corta en 12 ms,
  se reafina en silencio y vuelve a entrar. Su volumen es el POT1 de Melodías.
- **Clavecín**: el arpegio como pulso angosto, brillante y sin barrido de filtro.
- **Catedral**: reverb larga y oscura (~1.8 s). Entran la caja, el clap, los toms, las melodías, el
  arpegio y el colchón, cada escenario con su cantidad. Bombo, bajo y hats van secos.

Todo esto se ajusta en las tablas de arriba del `.ino`: `ESCENARIOS[]` (patrones, progresión, colchón,
catedral), `KITS[]`, `TBAJO[]`, `TMEL[]` y las constantes `CORO_*`, `CUERDAS_CORTE`, `REVERB_FB`.

## Pantalla OLED: la fiesta gótica

Opcional (SSD1306 128×64 en el conector OLED de la V2.0, SDA 21 / SCL 38; sin librería). El fondo es el
**lugar** del escenario — cripta, túnel, pista de baile, procesión, forja, salón — y adelante baila la gente:

| Suena | Se ve |
|---|---|
| Bombo | La mitad de la gente salta, la luz del lugar tira rayos y sube el contraste |
| Contratiempo | Salta la otra mitad |
| Caja / clap | Brazos arriba y un destello (la pantalla se invierte ~50 ms; `OLED_DESTELLO` lo apaga) |
| Hats | Los murciélagos aletean |
| Bajo | La luz del lugar crece con cada nota |
| Detenido | Todos quietos, sin murciélagos, pantalla tenue |

Los seis dibujos van dentro del `.ino` (entre `// >>> FONDOS` y `// <<< FONDOS`); `generar_fondos.py` los
dibuja, reescribe ese bloque y deja `fondos.png` para mirarlos. Pantalla de cabeza: `OLED_GIRO_180`.

## Controles

Los de matriz_oscura, salvo el BTN2 del General. BTN4 = panel siguiente y BTN5 = break / subida / drop en
todos los paneles.

| Panel | BTN1 | BTN2 | BTN3 | POT1 | POT2 | POT3 | POT4 |
|---|---|---|---|---|---|---|---|
| General | play/stop | **escenario** | fill | volumen | tempo | swing | pasa-bajos |
| Bajo | distorsión | patrón | timbre (8) | volumen | filtro | resonancia | caída |
| Batería | percusión extra | patrón | kit (7) | volumen | filtro | largo | densidad |
| Melodías | octava | patrón | timbre (7) | volumen | filtro | resonancia | caída |
| Oscilador | encender | timbre | octava | nota | filtro | LFO | volumen |
| Samples | titular | presentador | señal caída | volumen | pitch | reverb | eco |

## Cargar

Arduino IDE: placa **ESP32S3 Dev Module Octal (WROOM2)**, USB CDC On Boot **Enabled**, Flash Mode **DIO**,
Partition Scheme **Huge APP (3MB No OTA/1MB SPIFFS)**. Librería: **FastLED**.

## Estado

Compilado y verificado en una simulación en el PC (los 6 escenarios, sus 4 patrones, todo al máximo con y
sin samples, y 99 saltos de escenario al azar): 0 NaN, 0 notas fuera de la escala, el limitador sin
trabajar en uso normal y nada queda sonando después de Stop.

Cargado en la placa y **CPU medido** con `-DMEDIR=1` (el peor caso: densidad al máximo, oscilador, las 3
pistas de percusión extra, distorsión del bajo y un sample cada 1.5 s): promedio **62–78 %**, peor bloque
**85–111 %** (los más cargados, FORJA, CRIPTA y CLAUSTRO), que absorbe la cola de 12 ms del DMA. Son unos
6 puntos más que matriz_oscura (56–72 % / 72–103 %): el coro, las cuerdas y la catedral. BAILE y SALÓN se aprobaron escuchando
un render; PROCESIÓN y FORJA todavía no tienen comentario. El CPU no se volvió a medir con ellos ni con la pantalla. Si el LED 5 se pone magenta,
un bloque pasó del 80 % de su tiempo.
