# 📱 Cargar firmware en el PercuSynth desde el celular (sin PC)

Guía para flashear el PercuSynth (ESP32-S3) **directamente desde un teléfono
Android**, usando un cable **USB-C OTG** y una app de flasheo — sin Arduino IDE
ni computador.

> Probado y funcionando en un PercuSynth real (ESP32-S3, módulo con 8 MB PSRAM).

---

## 1. Qué necesitas

- **Teléfono Android** con soporte USB-Host (la mayoría de los modernos).
- **Cable / adaptador USB-C OTG** del teléfono al PercuSynth.
- App de flasheo: **ESPFlash** (*"ESPFlash - ESP32/ESP8266 flasher"*).
  👉 Es la que **escribe la flash de forma fiable** (usa *modo alta velocidad
  (stub)* + *compresión de firmware*: graba rápido y completo).
- El archivo **`firmware.bin`** del firmware que quieras (ver punto 2).

> ⚠️ **Evita la app `ESP32_Flasher`** (la del ícono de Espressif genérico): en
> nuestras pruebas decía *"done"* pero **no escribía nada** en la flash → el
> equipo arrancaba en bucle con `invalid header: 0xffffffff`. No es problema del
> firmware, es esa app. **Usa ESPFlash.**

---

## 2. Conseguir el `.bin`

### Opción A — ya está en el repo (lo más fácil)
Algunos firmwares traen su binario compilado y listo. Por ejemplo:

- `firmwares/pads_imu_leds/bin/firmware.bin`

Bájalo a tu teléfono (en GitHub: abre el archivo → **Download raw / Descargar**).

### Opción B — compilarlo en la nube con GitHub Actions
Si el firmware no trae `.bin`, o lo modificaste, GitHub lo compila por ti (sus
servidores tienen todo el toolchain — tu teléfono no necesita compilar nada):

1. En el repo → pestaña **Actions** → workflow **"Build pads_imu_leds firmware"**.
2. Botón **Run workflow** (o se dispara solo al cambiar el firmware).
3. Cuando termine (✅ verde), ve a la pestaña **Releases** → descarga el
   `firmware.bin` del release correspondiente.

> El workflow está en `.github/workflows/build-pads-imu-leds.yml`. Fija versiones
> **estables** del toolchain (ESP32 core **3.0.7** + **FastLED 3.9.0**) — usar
> "la última" de cada uno puede romper el audio/LEDs. Para compilar **otro**
> firmware, duplica ese workflow y cambia la ruta del `.ino`.

---

## 3. Flashear con ESPFlash (paso a paso)

1. Conecta el PercuSynth al teléfono por **USB-C OTG** y dale **permiso** al
   dispositivo USB cuando Android lo pida.
2. Abre **ESPFlash**.
3. **Select Chip:** `ESP32-S3`.
4. Confirma que estén activos **"modo alta velocidad (stub)"** y **"compresión
   de firmware"** (vienen así por defecto).
5. Agrega el archivo **`firmware.bin`** y selecciona el offset **`0x0`**
   (en ESPFlash el offset se elige de una **lista**, no se escribe a mano).
   - `firmware.bin` es la imagen *merged*: trae **todo** adentro (bootloader +
     particiones + boot_app0 + app), por eso va completo a **`0x0`**.
6. Pulsa **Flash**. Tarda poco (segundos a ~1 min) y muestra el progreso.
7. Al terminar: **desconecta del teléfono** y alimenta el equipo desde un
   **cargador USB de pared o power bank** (ver punto 4).

¡Listo! El PercuSynth arranca con el nuevo firmware.

---

## 4. ⚡ Alimentación: NO lo hagas sonar desde el teléfono

El puerto OTG del teléfono da **poca corriente** — alcanza para *flashear*, pero
**no** para hacer funcionar todo el equipo. Si lo dejas colgando del teléfono,
puede que el chip encienda pero **los 6 LEDs y el audio queden muertos** (se van
a *brownout*).

👉 Para usarlo, aliméntalo desde su **batería**, un **power bank** o un
**cargador de pared 5V (≥1 A)**.

---

## 5. Verifica que arrancó bien

`pads_imu_leds` al encender (con buena alimentación):

- Los **6 LEDs SMD** de la placa **respiran en cian** (Panel A).
- **BTN1** + subir **POT2** (volumen) → suena el pad.
- **BTN1+BTN5** = Panel B (arpegio, violeta) · **BTN2+BTN4** = Panel C (timbre, naranja).
- Mueve el equipo → el IMU barre el filtro.

---

## 6. Problemas frecuentes

| Síntoma | Causa / solución |
|---|---|
| `invalid header: 0xffffffff` en bucle (serial) | La app **no escribió** la app en la flash. Usa **ESPFlash** (stub + compresión), no `ESP32_Flasher`. |
| Dice "done" pero el equipo no hace nada | Misma causa: la app de flasheo no grabó. Cambia de app y reflashea. |
| Se queda en *connecting* | Modo bootloader manual: mantén **BOOT/GPIO0**, pulsa y suelta **RESET**, suelta **BOOT**, reintenta. |
| Enciende el ESP32 pero **sin LEDs ni audio** | Lo estás alimentando desde el teléfono → falta corriente. Usa cargador/power bank. |
| Falla la escritura al ~99% con compresión | Cable/OTG inestable, o la app no es fiable. ESPFlash suele resolverlo. |

### ¿Cómo leer el log serial desde el celular?
Instala **"Serial USB Terminal"** (Kai Morich), conecta por OTG, **115200 baud
8N1**, **Connect**, y pulsa **RESET** en la placa. Un arranque sano queda en
silencio (este firmware no imprime nada); un bucle muestra `rst:...` repetido.

---

## 7. Diagnóstico de la flash (avanzado)

La imagen `firmware.bin` (merged, 4 MB) tiene esta estructura — útil si quieres
flashear los trozos por separado:

| Componente | Offset | Archivo separado |
|---|---|---|
| Bootloader 2ª etapa | `0x0` | `bin/bootloader_0x0.bin` |
| Tabla de particiones | `0x8000` | `bin/partitions_0x8000.bin` |
| otadata (boot_app0) | `0xe000` | `bin/boot_app0_0xe000.bin` |
| Aplicación | `0x10000` | `bin/app_0x10000.bin` |

Si el equipo **ya tiene bootloader** y solo quieres actualizar el programa,
basta con grabar `app_0x10000.bin` en el offset **`0x10000`** (mucho más chico).
