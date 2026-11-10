# Simulador del PercuSynth

El PercuSynth V2.0 dibujado en el navegador, corriendo **el mismo `.ino`** que flashearías en la placa.
Está pensado para talleres online: en vez de mostrar la placa con una cámara y pelear con el audio de
Meet, se comparte una pestaña de Chrome con su audio y todos ven y oyen la placa.

En el taller se escribe un código nuevo (con Claude o a mano), se suelta el `.ino` en la página y la
placa lo corre en unos segundos. Y si hay una placa enchufada, **«cargar a la placa»** escribe ese mismo código en ella.

## Cómo se abre

```bash
python tools/simulador/servidor.py
```

y se abre **http://localhost:8794** en Chrome o Edge. La primera compilación tarda ~25 s, porque además
compila la placa virtual; las siguientes tardan entre 1 y 4 s.

Requiere **Emscripten** instalado en `~/emsdk` (o donde apunte la variable `EMSDK`):

```bash
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
```

```bash
cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
```

**La escena:** una mesa de nogal de noche, vista desde arriba, con la placa y la matriz 32×8 en su marco de
madera. Los LEDs alumbran la mesa a su alrededor (más lejos cuanto más brillan), y el monitor serie, el MIDI y
la configuración de la placa están dibujados sobre la mesa; la línea ámbar que une cada uno con su pieza sólo
aparece al acercar el puntero. Todo cabe en una pantalla: «pantalla completa» arriba a la derecha (en el
celular la pide horizontal; si el teléfono no deja, la mesa se gira sola cuando está vertical).
Para abrir directo un firmware: `http://localhost:8794/?fw=industrial_matriz`.

**En Meet:** «Presentar» → «Una pestaña» → elegir la del simulador y dejar marcado **«Compartir también
el audio de la pestaña»**. Así el sonido sale directo, sin pasar por la supresión de ruido del micrófono.

## Qué se puede hacer

| En la placa | Cómo |
|---|---|
| Cargar un firmware del repo | El selector de arriba (lista `firmwares/`) |
| Cargar un código nuevo | «abrir .ino» o soltar el `.ino` (y sus `.h`) sobre la página |
| Botones BTN1–5 | Clic, o las teclas `1`–`5` (se pueden mantener apretadas) |
| Perillas POT1–4 | Arrastrar hacia arriba o abajo, o la rueda del mouse. Muestran el valor 0–4095 |
| Piezos | Piezos de contacto con pinza y plug de 6.3 mm en los jacks del borde de arriba (GPIO 4–7). Clic en un jack vacío para enchufar uno, clic en el plug para sacarlo (o las casillas del panel). Clic en la pinza o las teclas `Q` `W` `E` `R` (jacks 1–4); `P` golpea el primero enchufado |
| Header I2C | El header es uno: ahí va el **IMU** (MPU6050, 0x68) **o** la **pantalla OLED** (SSD1306, 0x3C), y se elige en el panel «Placa» |
| IMU (MPU6050) | El cuadro del panel inclina la placa. «Golpe al piso» (o `G`) le da al acelerómetro una sacudida amortiguada de unos 50 ms; la fuerza (0,05–1,5 g) va en la barrita de al lado. Para `impact_chimes` y `seismic_drone` |
| Pantalla OLED | Se interpreta como el chip: comandos y datos por I2C, modos de direccionamiento, espejos `A0/A1` y `C0/C8`, línea de inicio, contraste, invertir, apagar. Con la OLED puesta, un **segundo monitor** debajo de la placa la muestra en grande, con un botón para **girarla 90°** (para los videos verticales de `oled_video_techno`) |
| microSD | Casilla «puesta» en el panel. La tarjeta guarda lo que grabe el sketch (`SD.h` / `FS.h`) y **sobrevive a reinicios y recompilaciones** (no a recargar la pestaña: lo que importe, se baja antes); «N archivos» abre la lista para bajarlos o borrarlos. Un `.wav` o `.txt` soltado sobre la página va a la tarjeta (`mezclador_pistas` lee `/cancion.wav`) |
| EXT1 / EXT2 | En el panel se enchufa un sensor en cada bornera (GPIO 3 y GPIO 9): **FSR** (mantener apretado el disco y arrastrar hacia arriba para apretar más; `F` = a fondo) o **LDR + láser** (mantener apretado el haz para cortarlo con la mano; `L`). Lecturas: FSR suelto ≈ 0, a fondo ≈ 4000; LDR con el láser ≈ 4060, cortado ≈ 3180 |
| DHT11 / DHT22 | También se enchufa en EXT1 o EXT2 desde el panel. El módulo se dibuja con sus **3 pines a la vista** (GND · DATA · VCC) y un cable de color derecho a cada tornillo de la bornera (GND · PIN · 3V3), con el destino escrito (`GND`, `GPIO 3`, `3V3`). Clic en el módulo: una tarjeta con el esquema de conexión, la línea de código, la temperatura y la humedad del aire, y un botón para **soplarle** (sube la humedad y un poco la temperatura, y vuelve sola en unos segundos). Funciona con `DHT.h` (Adafruit) y `DHTesp.h`, y con código que lea los pulsos a mano: el sensor contesta por su **protocolo real de un hilo**, así que con el DATA en otro pin la lectura falla igual que en la placa |
| Micrófono (INMP441) | «usar el del PC» en el panel: el micrófono del computador entra por `i2s_channel_read` en el formato que pida el sketch (32 bits como el INMP441, 16 bits, mono o estéreo, a 16 o 44,1 kHz). Si el código abre el micrófono, el enlace se marca en naranjo |
| Cables USB | Clic en un cable para enchufarlo o desenchufarlo. Sin ninguno, la placa se apaga |
| Monitor serie | `Serial` (USB nativo) y `Serial0` (COM, en gris). Se le puede escribir |
| MIDI | Reloj, Start/Stop y notas del DIN-5 (`Serial1` con TX en el 43) y de USB-MIDI. El menú del panel MIDI los **manda a un sinte de verdad** por Web MIDI (el navegador pide permiso la primera vez) |
| LEDs | Los 6 de la placa, el RGB del módulo (GPIO 48) y la tira externa si el código declara más de 6 en el GPIO 46 |
| Matriz LED | Si los LEDs de la tira forman un rectángulo (256 = 32×8, 400 = 20×20…), abajo se elige verla como **matriz**, con el orden del dato (por filas o columnas), **serpentina** y espejos X/Y, que tienen que coincidir con el `XY()` del sketch. Por defecto: 32×8 por columnas en serpentina (`industrial_matriz`) y 20×20 por filas sin serpentina (`trance_midi_leds`, `matrix_midi_anyma`) |
| Sonido | Activado desde el arranque (el navegador lo deja sonar recién con el primer clic o tecla en la página); «silenciar» lo apaga. El I2S del sketch suena por Web Audio a 44,1 kHz |
| Placa en grande | El botón grande abajo a la izquierda: sólo la placa, viva y tocable, a toda la pantalla. Se sale con `Esc` o llevando el puntero a la esquina superior derecha |
| Cargar a la placa | El otro botón grande (Chrome o Edge en computador): escribe el código elegido en un PercuSynth de verdad con Web Serial y esptool-js, por el **USB izquierdo**, el mismo de Arduino IDE. No hay que elegir nada en Herramientas: `placa.py` lee los ajustes del encabezado del `.ino` (USB Mode, PSRAM, Partition Scheme) y si no cabe usa Huge APP |
| Cómo se usa | «cómo se usa ⌄» en la barra (o la tecla `?`). Baja una cortina negra, la placa se agranda y de cada botón y perilla sale una línea ámbar hasta lo que hace en **el código cargado**, con su descripción y los pasos para empezar. **La placa sigue viva**: se toca, suena y prende sus LEDs mientras se lee, y el texto del control que se toca se enciende. En los firmwares con paneles o menú (`industrial_matriz`, `industrial_matriz_v2`, `bajo_8_pasos`, `trance_imu`…) la página sigue en qué panel está la placa con los mismos botones que lo cambian (los lee del encabezado) y muestra **sólo lo que hace cada control en ese panel**; arriba dice en qué panel estás, y con un clic se corrige si la cuenta se pierde. Se lee del encabezado del `.ino` (`guia.py`; lo que falte, del README de la carpeta), así que un código nuevo del taller también trae la suya si sigue la plantilla de `PROMPT_PARA_LA_IA.md`. `Esc` la cierra |

Al cargar **otro** firmware, la placa se arma sola con lo que su código espera: la OLED o el IMU en el
header, la microSD, y el FSR o el LDR en la bornera que diga su `#define …_PIN`. Lo que se cambie a mano
se respeta mientras se siga con ese mismo firmware.

Si un código no compila, el error aparece abajo con su número de línea, y la placa sigue corriendo el
código anterior, igual que en Arduino IDE.

## Lo que no hace (todavía)

- **WiFi**: los firmwares de IA (`asistente_*`, `sampler_ia`, `oscilador_ia`, `compositor_ia`) no compilan.
- Librerías externas fuera de FastLED, `DHT.h` y `DHTesp.h` (por ejemplo `MPU6050.h`, `ESP32Synth.h`). El IMU sí funciona leyendo sus registros por `Wire`, que es como lo hacen los firmwares del repo.
- **Los tiempos de los buses**: la OLED recibe un cuadro al instante (en la placa son ~12 ms a 800 kHz) y la
  microSD nunca se traba 100–300 ms como una tarjeta de verdad. Un código que en la placa pierde cuadros o
  bloques por eso, acá no los pierde.
- **No reproduce el presupuesto de CPU del ESP32-S3.** Un código que en la placa se queda sin tiempo
  (el chasquido del DMA vacío) acá puede sonar limpio. Lo que sí reproduce es todo lo que bloquea:
  un `delay()` dentro del loop de audio se oye como hueco, igual que en la placa, y el panel cuenta los
  **huecos de audio**.

De los 55 firmwares del repo compilan y corren 44; los otros 11 necesitan WiFi (7) o una librería de la lista (`MPU6050.h` en `MIDI_Drum` y `dub_siren`, `ESP32Synth.h` en los dos secuenciadores melódicos). **El selector sólo ofrece los que corren**, del más nuevo al más antiguo: `servidor.py` esconde los que incluyen una cabecera que la placa virtual no tiene (`hal/include` y las de C/C++ de Emscripten), así que si un día se agrega esa librería vuelven solos; y la fecha de cada uno es la más antigua entre su primer commit y la creación del `.ino`. Un código escondido igual se puede abrir con «abrir .ino» (muestra qué librería falta).

## Cómo funciona

No es un emulador del chip. `compilar.py` hace lo mismo que Arduino IDE con el `.ino` (junta los
archivos, agrega `#include <Arduino.h>` y genera los prototipos) y lo compila a **WebAssembly** con
Emscripten contra una **placa virtual** (`hal/`): las mismas funciones de Arduino, ESP-IDF y FreeRTOS que usa el sketch,
reimplementadas sobre el navegador.

- Cada tarea de FreeRTOS es un hilo real (`xTaskCreatePinnedToCore` → pthread), así que los firmwares de dos núcleos corren tal cual.
- `i2s_channel_write` bloquea mientras la cola de audio está llena, como el DMA: el tempo del sketch lo marca el audio.
- Botones, perillas, LEDs, Serial, MIDI y audio pasan por una memoria compartida (`hal/include/sim_bus.h`) que la página lee y escribe directamente.
- La pantalla OLED es la RAM del SSD1306 (1024 bytes) más su configuración en esa misma memoria; la página arma la imagen como la barre el chip (`hal/src/wire.cpp`).
- La microSD es un mapa de archivos en memoria (`hal/src/sd.cpp`) con un candado de giro, porque la página también la lee desde el hilo principal del navegador. Al apagar la placa la página se trae los archivos y al arrancarla se los devuelve: por eso la tarjeta sobrevive.
- El DHT11 contesta pin a pin (`core.cpp`): cuando el sketch baja la línea 18 ms y la suelta, `digitalRead` entrega la trama de 40 bits con los tiempos del datasheet. Como el navegador puede parar un hilo 50–100 µs (y se perderían bits de 26 µs), durante la trama ese hilo usa un reloj que no se deja saltar: `micros()` y `delayMicroseconds()` corren parejos, como en el ESP32.
- El micrófono del PC entra por un AudioWorklet a un anillo de la memoria compartida, e `i2s_channel_read` lo remuestrea a la frecuencia del sketch.
- La placa corre dentro de un iframe invisible (`motor.html`): desenchufar o reiniciar es recargarlo.

```
index.html      la placa dibujada y los paneles
motor.html      el iframe donde corre el sketch compilado
servidor.py     sirve la página (con las cabeceras COOP/COEP) y compila
guia.py         lee el encabezado del .ino y arma la guía «cómo se usa» (python guia.py <firmware> la imprime)
compilar.py     .ino → C++ → WebAssembly
placa.py        .ino → imagen para el ESP32-S3 real, con arduino-cli (build/_placa/<id>.bin); --todos los deja listos
esptool/        esptool-js 0.5.7 (Apache-2.0), servido desde acá porque la página aislada con COEP no deja cargarlo de otro sitio sin permiso
exportar_web.py la versión para gclabchile.com/simulador: todo ya compilado, para el simulador y para la placa
hal/            la placa virtual: Arduino.h, FastLED.h, Wire.h, driver/i2s_std.h, freertos/…
build/          compilaciones (se pueden borrar; no van al repo)
```

## Escribir código que corra aquí y en la placa

Sirve lo mismo que pide `PROMPT_PARA_LA_IA.md`: pines fijos, I2S con `driver/i2s_std.h`, LEDs con
FastLED en el GPIO 46 y el IMU leyendo registros por `Wire`. Si algo no existe en la placa virtual, el
compilador lo dice con el nombre de la librería que falta.
