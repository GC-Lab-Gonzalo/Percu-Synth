---
title: "Toqué el viaje: cargué el firmware de mi sintetizador en un bus, desde el celular y con IA"
date: 2026-06-23
author: "Gonzalo — GC Lab Chile"
tags: [percusynth, esp32, firmware, claude-code, diy, historia]
cover: img/bus-percusynth-portada.jpg
description: "La historia de cómo revivi mi PercuSynth en pleno bus, sin computador, pidiéndole a Claude Code desde el smartphone que me compilara y dejara listo el firmware para flashear por USB."
---

> 📸 **Fotos para intercalar** (reemplaza los `img/...` por tus archivos):
> `bus-entorno.jpg`, `usb-bus.jpg`, `percusynth-enchufado.jpg`,
> `pantallazo-chat-claude.jpg`, `pantallazo-espflash.jpg`, `metro-santiago.jpg`

# Toqué el viaje

No me gusta andar en auto. Me gusta el bus: me gusta mirar por la ventana, dejar
que el viaje pase, disfrutarlo. Hoy me tocó uno de esos buses amplios, cómodos…
y con un detalle que lo cambió todo: **un USB de carga disponible en el asiento.**

![El bus, los asientos, el viaje](img/bus-entorno.jpg)
*El escenario perfecto para tocar un rato.*

Así que hice lo natural para mí: saqué el **PercuSynth** de la mochila, lo enchufé
al USB del bus, me puse los audífonos… y me preparé para tocar el viaje.

![El USB de carga del bus](img/usb-bus.jpg)
*Un puerto USB en un bus. Para la mayoría, un cargador. Para mí, un escenario.*

## El balde de agua fría

Y ahí me acordé. **Ayer hice el taller de PercuSynth** y, para la demo, le había
cargado un firmware que solo funciona con una app de **Web Serial** desde el
navegador de un computador. O sea: el sinte que tenía en las manos **no me servía
de nada** sin una PC al lado. Y la PC, obvio, estaba en casa.

Iba con todas las ganas de tocar el `pads_imu_leds` —ese firmware de pads
profundos con arpegio y los LEDs reactivos que vive en el repositorio del
proyecto— y de repente no podía. Un momento decepcionante de verdad.

## La idea: ¿y si le pregunto a Claude?

Entonces se me cruzó algo: **tengo Claude en el celular.** ¿Y si le pido a
**Claude Code** que me cargue el firmware… desde el mismo smartphone?

Le escribí, tal cual:

> *"Crees que sea posible cargar un código del repo en el Percu-Synth desde este
> dispositivo, es un teléfono android"*

Y lo que vino después fue, sinceramente, **mágico**.

![La conversación con Claude Code en el celular](img/pantallazo-chat-claude.jpg)
*Pidiéndole a Claude Code que me sacara del problema, sentado en el bus.*

Lo primero que hizo no fue tirarme una respuesta de memoria, sino **revisar el
repositorio** para entender cómo se flashea. Me explicó el matiz clave: la
herramienta web del proyecto usa **Web Serial**, una API que **el Chrome de
Android no tiene** — por eso mi plan original era imposible. Pero acto seguido me
dio la salida: **sí se puede**, con un cable **USB-C OTG** y una app nativa de
flasheo en el teléfono.

El problema fino era que el firmware que yo quería existía en el repo solo como
código fuente (`.ino`), no como binario (`.bin`) listo para grabar. Y el teléfono
no compila. Acá viene lo que más me voló la cabeza:

## Compilar en la nube, flashear desde el bolsillo

Claude Code **montó un GitHub Action** que compila el firmware en los servidores
de GitHub —donde sí está todo el toolchain del ESP32— y publica el `firmware.bin`
ya listo en un *Release*, con un link directo que abro y descargo desde el
celular. Todo el trabajo pesado pasó **en la nube**; mi teléfono solo tuvo que
bajar un archivo.

Hubo un par de tropiezos en el camino (versiones de librerías, una app de
flasheo que decía *"listo"* pero en realidad no grababa nada…), pero los fuimos
resolviendo a pulso, leyendo hasta el log de arranque del chip por el puerto
serial. **Finalmente funcionó con una app llamada `ESPFlash`** — la que sí
escribe la memoria de forma fiable.

![Flasheando con ESPFlash desde el teléfono](img/pantallazo-espflash.jpg)
*Chip ESP32-S3, `firmware.bin` a `0x0`, y a grabar. Desde el celular.*

![El PercuSynth enchufado y andando](img/percusynth-enchufado.jpg)
*Revivido. Pads, arpegio y LEDs, listos para el viaje.*

Y ahí, en el bus, con los audífonos puestos, **por fin toqué.**

## Lo que esto significa

Hace unos años esto **habría sido imposible sin mi computador**. Hoy lo hice
entero desde el bolsillo. Es más: en algún momento pensé que Claude Code incluso
**podría usar el smartphone para grabar el chip directamente**, sin ninguna app
de por medio. Parece estar bloqueado por ahora —y no quise seguir indagando en
pleno viaje— pero intuyo que debiese poderse.

Y esto abre una posibilidad gigante. No es solo "flashear un binario que ya
existe". Es que **podría pedirle a Claude Code que me genere un código nuevo,
me lo compile y me entregue el archivo para cargarlo ahí mismo, desde el
teléfono.** El taller, el laboratorio, el "subir un firmware nuevo"… cabiendo
en un bus.

## Postdata: música en el Metro de Santiago

Como quedé picado, también probé el PercuSynth **en el Metro de Santiago**. Me
puse a hacer música ahí mismo. La gente miraba — claro, era raro ver a alguien
sacando sonidos de una cajita con LEDs en el metro — pero yo solo quería tocar.
Y toqué.

![Haciendo música en el Metro de Santiago](img/metro-santiago.jpg)
*El Metro como sala de ensayo. La gente mira; uno toca igual.*

---

## ¿Quieres hacer lo mismo?

Lo dejé documentado para que cualquiera lo repita. Necesitas:

- 📱 Un **teléfono Android** + cable/adaptador **USB-C OTG**.
- 🔌 La app **ESPFlash** (la que graba de verdad; evita `ESP32_Flasher`).
- 🐙 Una **cuenta de GitHub** con acceso al repo del proyecto (para compilar el
  firmware en GitHub Actions).
- 🤖 Una **cuenta de Claude** y la **app de Claude Code** en el celular.
- ⚡ Para *tocar* (no solo flashear): aliméntalo desde un **cargador o power
  bank** — el USB de un bus o del teléfono da poca corriente.

El procedimiento técnico paso a paso está en la guía del repositorio:
**`FLASHEAR_DESDE_CELULAR.md`**, y el `firmware.bin` de `pads_imu_leds` ya viene
precompilado en `firmwares/pads_imu_leds/bin/`.

El PercuSynth nació para ser un instrumento portátil y educativo. Hoy, gracias a
un USB en un asiento y a una IA en el bolsillo, fue exactamente eso: **portátil
de verdad.**

*— Gonzalo, GC Lab Chile*
