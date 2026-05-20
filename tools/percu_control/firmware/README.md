# PercuSynth Firmware — flasheo desde el navegador

Esta carpeta contiene el firmware compilado que el botón **FLASH FW** del panel de control instala directo en el ESP32-S3 vía USB, sin Arduino IDE.

## Archivos

| Archivo            | Qué es                                          |
|--------------------|-------------------------------------------------|
| `firmware.bin`     | Imagen completa (bootloader + partitions + app) |
| `manifest.json`    | Descriptor que lee ESP Web Tools                |

`firmware.bin` es la versión **merged** que genera Arduino IDE: contiene bootloader, tabla de particiones y aplicación combinados en una sola imagen, con los offsets correctos ya alineados. ESP Web Tools la flashea al chip en una sola pasada.

## Cómo regenerar el firmware

Cuando cambies algo en el `.ino`:

1. Abre el sketch en Arduino IDE (`percusynth_theredddmin.ino`).
2. Configura la placa:
   - **Tools → Board → ESP32 Arduino → ESP32S3 Dev Module** (o la variante exacta que uses)
   - **USB CDC On Boot:** Enabled
   - **Flash Mode:** DIO ← (crítico: si lo pones en OPI el audio I2S se rompe)
   - **Partition Scheme:** Default
3. **Sketch → Export Compiled Binary** (Ctrl+Alt+S).
4. Arduino te deja varios `.bin` en `tools/percu_control/build/esp32.esp32.<variante>/`. El que importa es:
   - `percusynth_theredddmin.ino.merged.bin` ← este
5. Cópialo a esta carpeta y renómbralo a `firmware.bin` (sobrescribe el anterior).

Si quieres automatizar, puedes correr esto cada vez que recompiles:

```powershell
# desde tools/percu_control/
Copy-Item "build/esp32.esp32.esp32s3-octal/percusynth_theredddmin.ino.merged.bin" "firmware/firmware.bin" -Force
```

## Servirlo localmente

ESP Web Tools fetchea el `manifest.json` y `firmware.bin` por HTTP. **No funciona abriendo `index.html` con doble click** (file://). Tienes que servir la carpeta `tools/percu_control/` con un mini-servidor:

```bash
cd "tools/percu_control"
python -m http.server 8000
```

Luego abres http://localhost:8000 en Chrome o Edge.

## Notas

- Solo funciona en **Chrome** o **Edge** (Web Serial API).
- Si cambias la variante de placa (Flash Mode, Partition Scheme, etc.) tienes que regenerar `firmware.bin`.
- Si más adelante quieres que un solo `.bin` sirva para distintas notas base sin recompilar, agrega un comando Serial tipo `BASE_HZ <valor>` que guarde en NVS — el panel ya puede mandarlo.
