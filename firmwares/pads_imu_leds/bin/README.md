# Binarios precompilados — pads_imu_leds (ESP32-S3)

Imágenes listas para **flashear desde el celular** (sin Arduino IDE).
Compiladas en GitHub Actions con **ESP32 core 3.0.7 + FastLED 3.9.0** y los
settings del firmware (ESP32S3 Dev Module · CDC On Boot · Flash DIO · OPI PSRAM).

> 📱 Procedimiento completo: ver [`FLASHEAR_DESDE_CELULAR.md`](../../../FLASHEAR_DESDE_CELULAR.md) en la raíz del repo.

## Forma rápida (recomendada)

Con la app **ESPFlash** (Android) → chip **ESP32-S3** → graba:

| Archivo | Offset |
|---|---|
| **`firmware.bin`** (todo en uno) | **`0x0`** |

`firmware.bin` es la imagen *merged*: trae bootloader + particiones + boot_app0
+ app. Es lo único que necesitas para una instalación limpia.

## Archivos por separado (avanzado)

Por si quieres grabar solo una parte (p. ej. actualizar **solo** el programa en
un equipo que ya tiene bootloader):

| Archivo | Offset | Qué es |
|---|---|---|
| `bootloader_0x0.bin` | `0x0` | Bootloader 2ª etapa |
| `partitions_0x8000.bin` | `0x8000` | Tabla de particiones |
| `boot_app0_0xe000.bin` | `0xe000` | otadata (selecciona el slot de arranque) |
| `app_0x10000.bin` | `0x10000` | La aplicación (el firmware en sí) |

> Para regenerar estos binarios: dispara el workflow
> `.github/workflows/build-pads-imu-leds.yml` (pestaña **Actions**) y descárgalos
> del **Release** `pads_imu_leds-fw`.
