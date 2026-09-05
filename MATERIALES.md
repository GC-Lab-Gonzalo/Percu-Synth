# PercuSynth — Lista de materiales

Todo lo necesario para armar un PercuSynth con la **placa V2.0**. La PCB fabricada es **opcional**: el
mismo circuito se puede armar en protoboard o en placa perforada, y corre exactamente el mismo firmware.

> Los componentes se listan con su **término de búsqueda** en vez de un enlace, porque los enlaces
> de tienda mueren y el término de búsqueda no. Con ese texto los encuentras en AliExpress,
> Mouser, o la tienda de electrónica de tu ciudad.

> **Qué cambió de la V1.1 a la V2.0 (septiembre 2026):** cada entrada de piezo lleva ahora un
> **diodo Schottky 1N5817**, un **condensador de 10 nF** y una resistencia de **100 kΩ** (antes era
> sólo una de 1 MΩ). Además la placa trae sitio para el **micrófono INMP441** y un conector para
> una **pantalla OLED** opcional. Si tienes una placa V1.1, sigue funcionando igual: sólo cambia
> lo que va en las entradas de piezo.

---

## 1. Módulos

| # | Componente | Cant. | Buscar como | Notas |
|---|---|---|---|---|
| 1 | **ESP32-S3 DevKitC-1 N16R8** | 1 | `ESP32-S3-DevKitC-1 N16R8` | El cerebro. **Tiene que ser S3** — el ESP32 clásico no sirve. La PSRAM (`R8`) hace falta para los firmwares con samples e IA. Trae dos puertos USB-C |
| 2 | **DAC PCM5102A** | 1 | `GY-PCM5102 I2S DAC` | La salida de audio estéreo por I2S. Ojo: es PCM510**2**A |
| 3 | **IMU MPU6050** | 1 | `GY-521 MPU6050` | Acelerómetro + giroscopio, por I2C |
| 4 | **Micrófono INMP441** | 1 | `INMP441 I2S microphone module` | Micrófono I2S. Lo usan los firmwares de IA (asistente, sampler, compositor). Va montado en la placa desde la V2.0 |

## 2. Conectores

| # | Componente | Cant. | Buscar como | Notas |
|---|---|---|---|---|
| 5 | **Jack 6.3 mm hembra mono, montaje en PCB** | 4 | `6.35mm mono jack socket PCB` (huella NMJ4HFD2) | Entradas MIC1–MIC4: es por donde entran los piezos |
| 6 | **Conector DIN-5 hembra para PCB** | 1 | `DIN 5 pin female PCB socket MIDI` | Salida MIDI OUT |
| 7 | **Bornera de tornillo para PCB, 3 vías** | 3 | `KF128 3P` (fija) o `KF301 3P` (enchufable) | Sensores externos EXT1 / EXT2 y salida a tira LED. ⚠️ Mide el **paso** en la placa antes de comprar: 2.54, 3.5 o 5.08 mm |
| 8 | **Bornera de tornillo para PCB, 2 vías** | 1 | `KF128 2P` / `KF301 2P` | Entrada de 5 V. Mismo tipo y mismo paso que el anterior |

> **El nombre del conector 7/8:** en español es *bornera* o *bloque terminal de tornillo para PCB*;
> en inglés, *PCB screw terminal block*. Lo que define la compra son tres cosas: número de vías
> (2P, 3P…), **paso** entre pines (2.54 / 3.5 / 5.08 mm) y si es fija (KF128, se atornilla directo)
> o enchufable (KF301, la parte de arriba sale). Las verdes de la placa son fijas.

## 3. Controles y luces

| # | Componente | Cant. | Buscar como | Notas |
|---|---|---|---|---|
| 9 | **Potenciómetro 1 kΩ lineal, montaje en PCB** | 4 | `potentiometer B1K vertical PCB 9mm` | **Lineal (B)**, no logarítmico. El valor no es crítico: 1 k, 10 k o 50 k funcionan igual como divisor hacia el ADC. Que sean todos iguales. Huella de 9 mm vertical (R09V) |
| 10 | **Switch de teclado mecánico (compatible MX)** | 5 | `mechanical keyboard switch MX` | Los 5 botones. Elige el tacto que te guste (clicky, táctil o lineal) |
| 11 | **Keycaps** | 5 | `MX keycaps` | Las tapas de los switches |
| 12 | **LED WS2812B SMD 6028** | 6 | `WS2812B 6028` | Los 6 LEDs de la placa. Son los índices 0–5, por eso los firmwares dibujan desde `START_LED = 6`. ⚠️ La huella de la V2.0 es **6028** (6.0 × 2.8 mm), no 3528: confirmar contra el LED que se compró |

## 4. Componentes discretos

Todas las resistencias son de **1/4 W axiales**. Los diodos y el electrolítico **tienen polaridad**;
las resistencias y los cerámicos no.

| # | Componente | Cant. | En la placa | Notas |
|---|---|---|---|---|
| 13 | **Resistencia 220 Ω** | 2 | R10, R11 | Circuito de salida MIDI |
| 14 | **Resistencia 1 MΩ** | 5 | R1–R5 | Pull-up de los 5 botones |
| 15 | **Resistencia 100 kΩ** | 4 | R6–R9 | Carga de cada entrada de piezo. **Nuevo en V2.0** (antes 1 MΩ) |
| 16 | **Diodo Schottky 1N5817** | 4 | D1–D4 | Protección de cada entrada de piezo. Encapsulado **DO-41** (axial). Buscar como `1N5817 DO-41`. La **franja** del cuerpo es el cátodo y va donde lo marca la serigrafía. **Nuevo en V2.0** |
| 17 | **Condensador cerámico 10 nF** | 4 | C2–C5 | Filtro de cada entrada de piezo. Código `103`, radial, paso 2.54 mm. Buscar como `10nF ceramic capacitor 103`. Sin polaridad. **Nuevo en V2.0** |
| 18 | **Condensador electrolítico 100 µF** | 1 | C1 | Desacoplo de la alimentación del MPU6050. Va en 3.3 V, así que sirve cualquiera de **6.3 V o más** (el original es de 16 V). Diámetro 5 mm, paso 2 mm. La pata larga (+) va al 3.3 V |

> **Por qué el piezo lleva diodo y condensador.** Un disco piezoeléctrico golpeado genera picos de
> decenas de voltios, de las dos polaridades. El ADC del ESP32-S3 sólo lee entre 0 y 3.3 V y no
> tolera tensión negativa. El **100 kΩ** descarga el piezo a masa, el **1N5817** recorta el semiciclo
> negativo (un Schottky conduce desde ~0.3 V, antes de que el pin sufra) y el **10 nF** estira el
> golpe unos milisegundos para que el ADC alcance a verlo aunque el `loop()` esté ocupado con el
> audio. Los tres van en paralelo entre la señal del piezo y GND. En la V1.1 sólo había la
> resistencia, y los golpes fuertes se perdían o llegaban recortados.

## 5. Pin headers

Los módulos **no se sueldan directo a la placa**: se montan sobre tiras hembra, así se pueden
sacar y reutilizar.

| # | Componente | Cant. | Para |
|---|---|---|---|
| 19 | Tira hembra 22 pines | 2 | ESP32-S3 DevKitC-1 (44 pines, 22 por lado) |
| 20 | Tira hembra 8 pines | 1 | MPU6050 |
| 21 | Tira hembra 6 pines | 1 | PCM5102A |
| 22 | Tira hembra 3 pines | 1 | PCM5102A |
| 23 | Tira hembra 4 pines | 1 | Conector OLED. Sólo si vas a poner la pantalla (ver opcionales) |

> El **INMP441** es la excepción: va soldado con sus propios pines (dos filas de tres) en el círculo
> que marca la serigrafía, junto al ESP32.

> Se compran como `2.54mm female pin header strip` y se cortan al largo necesario. Los módulos
> suelen venir con sus tiras **macho** incluidas; si no, agrega `2.54mm male pin header`.

## 6. Para que suene y funcione

| # | Componente | Cant. | Notas |
|---|---|---|---|
| 25 | Cable **USB-C de datos** | 1 | De datos, no de sólo carga. Alimenta y programa la placa |
| 26 | Salida de audio | — | Parlante amplificado, interfaz de audio, audífonos o entrada de línea del PC |

## 7. Opcionales (según lo que quieras hacer)

| Componente | Buscar como | Para qué |
|---|---|---|
| **Discos piezoeléctricos** con plug 6.3 mm | `piezo disc transducer 27mm` | Pads de percusión por impacto. Se conectan a MIC1–MIC4 |
| **Pantalla OLED 0.96" I2C (SSD1306, 4 pines)** | `0.96 OLED I2C SSD1306 4 pin` | Va en el conector OLED de la V2.0 (mismo bus I2C del MPU6050: SDA 21 / SCL 38). ⚠️ Ningún firmware la usa todavía; el conector está para los que vengan. Revisa que el orden de pines del módulo (GND / VCC / SCL / SDA) coincida con la serigrafía |
| **Tira LED WS2812B** | `WS2812B LED strip` | Visualizadores. El largo lo eliges tú (30, 60, 68, 144…) y se ajusta en el firmware |
| **LDR + resistencia 220 Ω** | `LDR 5528 photoresistor` | Sensor externo. Es lo que usa `laser_chimes` para detectar el corte de un láser |
| **Cable MIDI DIN-5** | `MIDI cable DIN 5 pin` | Para conectar la salida MIDI a un sinte o caja de ritmos externa |
| **Batería USB (power bank)** | — | Para tocar sin computador |
| **Caja de madera o impresa en 3D** | — | El PercuSynth de las fotos va montado en una caja de madera |

## 8. Herramientas (sólo si vas a soldar)

- Soldador de temperatura regulable y su soporte
- Estaño con núcleo de resina, y flux
- Esponja o lana de bronce para limpiar la punta
- Alicate de corte y pinza
- Malla desoldadora o chupa-estaño
- Multímetro — para revisar continuidad y cortos **antes** de conectar el USB
- Buena luz, y lupa si vas a soldar los LEDs SMD

---

## Los tres caminos para armarlo

| | PCB fabricada | Protoboard | Placa perforada |
|---|---|---|---|
| Soldadura | Sí | No | Sí |
| Tiempo hasta tenerlo | Lo que demore el envío | Inmediato | Días |
| Robustez | Alta | Baja (se sueltan los cables) | Media-alta |
| LEDs SMD 0–5 | Los trae | No existen | No existen |

**Si no usas la PCB:** los firmwares con luces empiezan a dibujar en `START_LED = 6` porque asumen
los 6 LEDs SMD de la placa. En un armado propio la tira empieza en el índice 0, así que cambia esa
constante (o suelda 6 LEDs de sacrificio al inicio de la tira).

También cambian los componentes que necesitas: en protoboard no hacen falta las borneras, los jacks
de 6.3 mm ni los LEDs SMD — los sensores se conectan con cables directo a los pines. Lo que **sí**
conviene mantener aunque no uses la PCB es el trío de cada piezo (100 kΩ + 1N5817 + 10 nF a GND):
sin él un golpe fuerte puede dañar el pin del ESP32.

## Pedir la PCB en JLCPCB

1. Descarga el repositorio (GitHub → `Code` → `Download ZIP`).
2. En `Hardware/` está `Gerber_Percu-synth_1-PCB_PCB_Percu-synth_V2.0.zip`. **Ese ZIP se sube tal
   cual, sin abrirlo.** (El `V1.1` sigue en la carpeta sólo como referencia de las placas antiguas;
   para fabricar usa la V2.0.)
3. En jlcpcb.com, *Add gerber file*.
4. Las opciones que importan: cantidad (el mínimo suele ser 5), color de máscara, espesor y acabado.
   El resto se deja como viene.
5. Elige el envío según tu país mirando el plazo real antes de pagar.

La placa mide **200 × 125 mm**.

---

## Pinout de referencia

Para armarlo sin PCB, esta es la única tabla que importa:

| Señal | Pin ESP32-S3 |
|---|---|
| I2S LCK (LRCK) → PCM5102A | 39 |
| I2S DIN (DATA) → PCM5102A | 40 |
| I2S BCK (BCLK) → PCM5102A | 41 |
| Botones 1–5 | 44, 42, 0, 45, 47 |
| Potenciómetros 1–4 | ADC 1, 2, 8, 10 |
| Piezos 1–4 (MIC1–MIC4) | ADC 4, 5, 6, 7 |
| Sensores externos A / B | ADC 3 / 9 |
| Datos LED WS2812 | 46 |
| MIDI DIN-5 TX | 43 |
| I2C SDA / SCL (MPU6050 y OLED) | 21 / 38 |
| INMP441 WS / SCK / SD | 11, 12, 13 |

⚠️ **BTN3 es el GPIO 0**, que además es el botón de boot: funciona como botón normal, pero no lo
dejes apretado al energizar.

---

*GC Lab Chile — PercuSynth. Lista al día con el esquemático V2.0 (2026-09-04). Los ⚠️ son datos por confirmar.*
