# Handoff → terminar el post en tu PC (Claude Code local)

Todo lo necesario para retomar este blog en tu computador, dentro de la carpeta
de tu sitio (`C:\Users\gsces\OneDrive\Documentos\GC Lab Chile\web`), con Claude
Code **local** (el que sí ve tus archivos).

---

## Paso 1 — Lleva estos 2 archivos a tu carpeta web

Descárgalos del repo (botón *Download raw* en GitHub, o `git pull` si tienes el
repo clonado) y cópialos a tu proyecto web:

1. **El borrador del post:**
   `blog/2026-06-23-firmware-en-el-bus.md`
   Raw: `https://raw.githubusercontent.com/GC-Lab-Gonzalo/Percu-Synth/claude/lucid-pasteur-s2beae/blog/2026-06-23-firmware-en-el-bus.md`

2. **La guía técnica** (para enlazar desde el post, opcional):
   `FLASHEAR_DESDE_CELULAR.md`
   Raw: `https://raw.githubusercontent.com/GC-Lab-Gonzalo/Percu-Synth/claude/lucid-pasteur-s2beae/FLASHEAR_DESDE_CELULAR.md`

---

## Paso 2 — Ten listas las 6 fotos

Ponlas en una carpeta de imágenes con estos nombres (o dile a Claude local que
las renombre por ti). Cada una ya tiene su pie de foto en el borrador:

| Nombre esperado | Foto |
|---|---|
| `bus-percusynth-portada.jpg` | portada (la mejor) |
| `bus-entorno.jpg` | el bus / los asientos |
| `usb-bus.jpg` | el conector USB del bus |
| `percusynth-enchufado.jpg` | el PercuSynth andando |
| `pantallazo-chat-claude.jpg` | la conversación con Claude |
| `pantallazo-espflash.jpg` | flasheando con ESPFlash |
| `metro-santiago.jpg` | tocando en el Metro |

---

## Paso 3 — Abre Claude Code local en la carpeta web y pega esto

> Estoy en la carpeta de mi sitio web de GC Lab Chile. Quiero publicar **mi primer
> post de blog**. Tengo el borrador en `2026-06-23-firmware-en-el-bus.md` (ya lo
> copié a esta carpeta) y 6 fotos que te indico. Por favor:
> 1. **Detecta el stack** de mi sitio (Jekyll / Hugo / Astro / Next / WordPress /
>    HTML plano) mirando los archivos del proyecto.
> 2. **Adapta el post a ese stack**: mueve el `.md` a la carpeta de posts que
>    corresponda, ajusta el *front-matter* (o conviértelo a HTML si el sitio no
>    usa Markdown), y arregla el *slug*/URL.
> 3. **Coloca las imágenes** en la carpeta correcta del sitio y corrige las rutas
>    `img/...` del post para que apunten ahí.
> 4. Déjalo **listo para previsualizar en local** y dime el comando para verlo.
> 5. Pregúntame lo que necesites (stack, rutas, fecha, autor, categoría).

---

## Decisiones que te va a preguntar (tenlas pensadas)

- **Stack del blog** (si lo sabes, díselo de una: Jekyll/Hugo/Astro/WordPress/HTML).
- **Slug/URL** del post (sugerencia: `firmware-en-el-bus` o `toque-el-viaje`).
- **Categoría/tags** (ej.: *historias*, *percusynth*, *diy*).
- ¿Enlazas la guía técnica `FLASHEAR_DESDE_CELULAR.md` o la dejas solo en el repo?

---

## Estado actual (lo que ya quedó hecho en el repo Percu-Synth)

- ✅ `blog/2026-06-23-firmware-en-el-bus.md` — borrador del post con storytelling.
- ✅ `FLASHEAR_DESDE_CELULAR.md` — guía de flasheo desde el celular.
- ✅ `firmwares/pads_imu_leds/bin/` — `firmware.bin` (funcionando) + binarios separados.
- ✅ `.github/workflows/build-pads-imu-leds.yml` — compila el firmware en la nube.

Todo en la rama **`claude/lucid-pasteur-s2beae`**.
