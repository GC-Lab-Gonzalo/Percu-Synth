# percu_control — Panel universal + flasheo desde el navegador

App web standalone con interfaz visual completa para configurar el PercuSynth: osciladores (5 formas de onda), octave shift, mixer, ruido (white/pink), drive multimodo (off/soft/fold/bit), filtro, LFO y master. **Incluye un botón `⚡ FLASH FW` que instala el firmware compilado directo al ESP32-S3 desde el navegador** vía ESP Web Tools, sin necesidad de Arduino IDE.

Pensada como la cara visible del PercuSynth para usuarios finales y demos: bajan el .zip, abren `index.html` con un mini-servidor, conectan la placa por USB y pueden flashear + tocar sin tener nada instalado.

## Cómo correrla

ESP Web Tools **necesita HTTP** (no funciona con `file://`):

```bash
cd tools/percu_control
python -m http.server 8000
```

Luego abrí <http://localhost:8000> en **Chrome o Edge** (la Web Serial API no existe en Firefox/Safari).

## Cómo se comunica con la placa

Dos canales independientes sobre USB:

| Acción              | Protocolo                                                  |
|---------------------|------------------------------------------------------------|
| `⚡ FLASH FW`        | ESP Web Tools → flash binario completo al ESP32-S3        |
| Mover knobs/botones | Web Serial → comandos texto (`FREQ`, `VOL`, etc.)         |

## Estructura

```
percu_control/
├── index.html             # la app entera (HTML + CSS + JS inline)
├── assets/
│   └── gclab-logo.png
├── firmware/
│   ├── firmware.bin       # imagen merged (bootloader + partitions + app)
│   ├── manifest.json      # descriptor que lee ESP Web Tools
│   └── README.md          # cómo regenerar firmware.bin desde Arduino IDE
└── build/                 # carpeta de export de Arduino (se sobrescribe)
```

Si tocás el `.ino` del firmware base, hay que regenerar `firmware/firmware.bin` — ver [firmware/README.md](firmware/README.md) para el paso a paso.

## Flujo típico de uso

1. `python -m http.server 8000` desde esta carpeta.
2. Abrir <http://localhost:8000> en Chrome.
3. Conectar el PercuSynth por USB-C.
4. (Primera vez) Apretar **⚡ FLASH FW** → seleccionar puerto → esperar ~30 s.
5. Apretar **Connect Serial** → mover knobs y disparar notas desde el navegador. El PercuSynth queda sonando incluso después de desconectar la web — la web es solo para configurar.

## Notas

- Solo funciona en **Chrome** o **Edge** (Web Serial + Web Bluetooth API).
- El primer flasheo borra todo el chip (`new_install_prompt_erase: true`).
- Si cambias `Flash Mode`, `Partition Scheme` u otras opciones críticas, hay que recompilar el `.bin`.
