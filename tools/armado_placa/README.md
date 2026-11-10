# Armado de la placa

La PCB V2.0 del PercuSynth en el navegador, pieza por pieza, para la clase en que se suelda: cada
componente se saca y se vuelve a poner, y al tocarlo dice qué es, qué hace en el PercuSynth, cómo se
reconoce (bandas, polaridad) y cómo se suelda. Sus pistas se encienden en ámbar hasta el pin del ESP32
donde llegan, con el número de GPIO.

Sigue la línea del simulador (la misma mesa de nogal de noche, ámbar y Archivo), pero la placa no es la
foto: se dibuja desde el diseño de EasyEDA, así que los pads, agujeros, pistas y serigrafía están donde
están de verdad y la placa se puede ver vacía, por abajo y con el cobre a la vista.

## Cómo se abre

Publicada en **https://gclabchile.com/armado/** (copia en `web/public_html/armado/`, con un `.htaccess` sin caché larga). Para actualizarla, copiar `index.html`, `pcb_v2.js` y `esquema_v2.js` a esa carpeta y subirlos por SSH al mismo lugar.

Doble clic en `index.html` (funciona desde el disco, sin servidor) o:

```bash
python -m http.server 8798 --directory tools/armado_placa
```

## Qué se puede hacer

| | |
|---|---|
| Ver qué es una pieza | Clic en la pieza (o en su lugar vacío). El panel dice qué es, su papel en el PercuSynth, cómo se reconoce, cómo se suelda y a qué va cada pin; sus redes se encienden en ámbar |
| Sacar una pieza | Arrastrarla fuera de la placa, o «sacar» en el panel. Queda en su montón sobre la mesa |
| Poner una pieza | Arrastrarla desde su montón hasta su lugar (los lugares libres se marcan y la pieza se calza al acercarla), o un clic en el montón: va al lugar elegido o al primero libre |
| Equivocarse | Soltar una pieza en el lugar de otra no la pone y dice qué va ahí («Ahí no: en R1 va una resistencia de 1 MΩ») |
| Pines del ESP32 | Clic en el ESP32 o en sus tiras hembra: los 44 pines rotulados con su GPIO y lo que hay conectado; al pasar el puntero por uno en ámbar se enciende su pista hasta la pieza |
| Dar vuelta la placa | «dar vuelta la placa» o `V`: la cara de abajo, con el estaño de cada pata soldada, las pistas de abajo y los LEDs, que van por ese lado |
| Ver las pistas | «ver pistas» o `P`: el cobre de la cara que se mira, y el de la otra punteado |
| Paso a paso | Parte con la placa vacía y la arma en el orden en que se suelda, de lo más bajo a lo más alto: LEDs (por abajo), resistencias, diodos, condensadores de 10 nF, micrófono, el de 100 µF, tiras hembra, switches, potenciómetros, borneras, jacks, MIDI, los módulos, que se enchufan, y al final las teclas, que se calzan a presión. Cada paso marca sus lugares y sus montones |
| Vista «esquema» | El esquemático real de EasyEDA, sin hoja ni marco, directo sobre la mesa y usando todo el espacio. Lo que se elige en una vista queda elegido en las otras: una pieza tocada en la placa aparece marcada en el esquema con sus cables en ámbar, y al revés. El panel muestra además la pieza en la otra vista (su símbolo cuando se mira la placa; su lugar en la placa cuando se mira el esquema o los cables) |
| Vista «cables» | Las mismas piezas sin PCB, unidas con cables de colores: el ESP32 al centro y cada pieza a la altura de los pines donde llega, cada cable por su carril. Colores por función (botones amarillo, perillas verde, piezos morado, DAC azul, I2C blanco y gris, micrófono celeste, LEDs rosado, sensores café, MIDI turquesa); GND, 3,3 V y 5 V terminan en una ficha de color en cada pin en vez de tirarse hasta el ESP32, y los LEDs cuelgan de dos rieles. **Se arma y desarma como la placa** («armar todo», «desarmar todo», o una pieza a la vez con «sacar»/«poner» en el panel): las piezas que faltan quedan como un fantasma punteado en su lugar y un cable sólo aparece cuando están puestas las piezas que une, así se revisan las conexiones una por una. Es el mismo armado que la placa: lo que se saca en una vista falta en la otra. Tocar un cable lo elige en las tres vistas y el panel dice qué une («Une R6 pin 2, D1 pin 2, C2 pin 2, JACK1 pin 1 y ESP32 GPIO 4») |
| Cablear uno a uno (cables) | Aparte de las piezas, los cables también se ponen y se sacan: «quitar cables» deja sólo las piezas, y cada pin libre muestra un **anillo** del color de su cable; tocarlo tira ese cable (si su otra punta todavía no está puesta, queda colgando y el aviso dice qué falta). Tocar un cable y «quitar este cable» lo saca; «cablear todo» los vuelve a tirar todos. GND, 3,3 V y 5 V terminan en una ficha redonda de su color (negra con el símbolo de tierra, naranja 3V3, roja 5V): todas las fichas iguales están unidas, y se tiran de una vez |
| Zoom (esquema y cables) | Rueda del mouse para acercar o alejar alrededor del puntero, arrastrar para moverse, doble clic sobre una pieza para acercarse a ella y doble clic sobre la mesa para volver a verlo todo. «acercar» en el panel va directo a la pieza elegida |

Las reglas de la placa real se respetan: un módulo no se enchufa sin su tira hembra, una tira no se
saca con el módulo puesto, la tecla va separada del switch (se calza en su cruz al final y hay que sacarla antes de sacar el switch), y el IMU y la pantalla OLED comparten el lugar (va uno u otro). La pantalla y
su tira de 4 son opcionales y no cuentan para «placa completa»; con todo lo demás puesto, los seis LEDs
se prenden.

## Cómo está hecho

```
index.html       la escena, las piezas dibujadas, los textos y la interacción
pcb_v2.js        la geometría de la PCB (generado; no se edita a mano)
extraer_pcb.py   Hardware/PCB_Percu-synth_2026-09-04.json V2.json → pcb_v2.js
esquema_v2.js    el esquemático como SVG enlazado con la PCB (generado; no se edita a mano)
extraer_esquema.py   el esquemático EasyEDA más nuevo de Hardware/ → esquema_v2.js
```

`extraer_pcb.py` lee el diseño EasyEDA (formato estándar) y deja, con el origen en la esquina del contorno
y en unidades de EasyEDA (10 mil = 0,254 mm): el contorno, los agujeros, las pistas y vías de cada cara
con su red, la serigrafía de las dos caras (textos incluidos, que EasyEDA guarda como trazos) y, por
componente, su designador y sus pads con la red de cada uno. Si cambia la PCB, se exporta el JSON de
EasyEDA a `Hardware/` y se corre:

```bash
python tools/armado_placa/extraer_pcb.py
```

En `index.html`, `LUGARES` arma cada lugar donde va una pieza a partir de esos componentes (las tiras
hembra del ESP32 y del DAC salen de partir sus pads), `TIPOS` tiene los textos de cada tipo de pieza y
`papel()` el de cada lugar. Los datos que no están en la PCB salen de `MATERIALES.md` (valores, códigos,
polaridad) y del pinout de `PROMPT_PARA_LA_IA.md`.

Dos cosas de la placa que se ven acá y no en la foto: los **LEDs WS2812B van por la cara de abajo**
(son de montaje invertido y alumbran por una ventana en la placa), y el ESP32 tiene **dos filas de
agujeros por lado**: la de adentro es la del DevKitC-1 y la de afuera (H1/H2) es para DevKits más anchos.

Y una que no está ni en la PCB ni en la foto: la placa sólo le lleva al **DAC PCM5102A** 3,3 V, GND, LCK,
DIN y BCK, así que su configuración depende de los **puentes de soldadura del propio módulo**, que muchos
traen abiertos: atrás H1L (FLT) a L, H2L (DEMP) a L, H3L (XSMT) a **H** y H4L (FMT) a L, y adelante SCK a
GND. Con XSMT abierto o en L el DAC queda mudo y todo lo demás funciona (le pasó a un alumno del taller
online). El panel del DAC trae la tabla y cómo medirlos con el multímetro, y el paso «Los módulos» lo
recuerda.

**El esquema** sale de exportar el esquemático en EasyEDA con *Archivo → Exportar → EasyEDA Source* y
dejar el `.json` en `Hardware/` (el script toma el más nuevo cuyo nombre empiece con `SCH` o diga
*schematic* / *esquema*). Después de exportar, o si cambió la PCB, se corre `extraer_pcb.py` primero y
`extraer_esquema.py` después:

```bash
python tools/armado_placa/extraer_esquema.py
```

El enlace entre las dos representaciones no es a mano: `extraer_esquema.py` sigue la conectividad del
esquemático (cables, uniones en T y etiquetas con el mismo nombre) y nombra cada red mirando qué pad de
la PCB toca, porque el pin *n* de R6 en el esquema es el pad *n* de R6 en la placa. Si una pieza está en
la PCB y no en el esquema, el script lo avisa.

La vista de cables se ubica a mano en `CAB` (dentro de `index.html`): posición, giro y, cuando hace falta,
hacia dónde sale cada pin y qué largo tiene su primer tramo, para que los cables no se pisen. Los carriles
los reparte solo: los que bajan hacia el ESP32 primero (el de más arriba, más cerca del módulo) y después
los que suben, así casi no se cruzan.
