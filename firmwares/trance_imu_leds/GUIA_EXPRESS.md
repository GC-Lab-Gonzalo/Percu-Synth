# Guía express — trance_imu_leds

Secuenciador de trance polifónico con visualizador en los **6 LEDs de la placa**.

---

## 1. Flashear (Arduino IDE)

1. Abre `trance_imu_leds.ino`.
2. Board: **ESP32S3 Dev Module** · USB CDC On Boot: **Enabled** · Flash Mode: **DIO** · PSRAM: **OPI PSRAM**.
3. Librerías: **FastLED** (gestor de librerías) — `Wire` e I2S vienen con el core ESP32 ≥ 3.x.
4. Compila y sube. No necesitas Serial.

---

## 2. Arrancar (30 segundos)

1. **BTN1** → Play. Suena la secuencia y los LEDs empiezan a moverse.
2. **POT3** → tempo (40–300 BPM).
3. **POT4** → cola/decay (súbelo para textura densa y polifónica).
4. **Mueve el equipo en el aire** → el IMU abre/cierra el filtro (*filter sweep*) y a la vez cambia el color de los LEDs.
5. **BTN2** cambia escala · **BTN3** cambia patrón · **BTN5** cambia octava.

---

## 3. Los 3 paneles

El color de los LEDs te dice en qué panel estás:

| Panel | Cómo entrar | Color LEDs | Para qué |
|---|---|---|---|
| **A** normal | (al encender) | 🟦 cian/azul | tocar, tempo, escala, patrón, octava |
| **B** notas | **BTN1 + BTN5** juntos | 🟪 violeta | cambiar tonalidad y armar el acorde |
| **C** timbre | **BTN2 + BTN4** juntos | 🟧 naranja | forma de onda, capas, detune, drive |

> El mismo combo te devuelve al Panel A. Un flash confirma el cambio.
> Al cambiar de panel los pots quedan **congelados**: cada uno retoma el control sólo cuando lo **mueves**.

---

## 4. Qué hace cada control por panel

### Panel A (cian) — tocar
| Control | Función |
|---|---|
| BTN1 | Play / Stop |
| BTN2 | Escala (Mayor · Menor · Armónica · Árabe · Lidia · Frigia) |
| BTN3 | Patrón (16) |
| BTN4 | Largo de secuencia (4 → 8 → 16 pasos) |
| BTN5 | Octava (-1 → 0 → +1 → +2) |
| POT1 | Ataque · POT2 Volumen · POT3 Tempo · POT4 Decay |

### Panel B (violeta) — tonalidad / acorde
| Control | Función |
|---|---|
| BTN1 F#(+7) · BTN2 C#(+2) · BTN3 B/tónica · BTN4 D#(+4) · BTN5 G#(+9) | cambia la tonalidad |
| POT1–POT4 | grado de escala (0–7) de cada una de las 4 voces del acorde |

> La nota y el acorde **persisten** al volver al Panel A.

### Panel C (naranja) — timbre
| Control | Función |
|---|---|
| BTN1 | Forma de onda (Sierra → Cuadrada → Triangular) |
| BTN2 | Octava arriba on/off |
| BTN3 | Unison (lead mono gordo) / Poly (acorde) |
| BTN4 | Quinta (power-chord) on/off |
| BTN5 | Sub-osc (-1 octava, graves) on/off |
| POT1 Detune · POT2 Drive · POT3 Piso de cutoff · POT4 Resonancia base |

> Todo el timbre **persiste** al volver al Panel A.

---

## 5. Qué muestran los 6 LEDs

- **Color** = panel (cian/violeta/naranja) + el filtro del IMU lo desplaza al moverte.
- **Barra (cuántos encienden)** = energía polifónica (más acorde/cola → más llena).
- **Destello** = cada beat; el downbeat (1, 5, 9, 13) pega más fuerte.
- **Detenido** = respiración lenta en el color del panel.

---

## 6. IMU (en los 3 paneles)

- Inclinar/mover en eje **X** → frecuencia de corte (cutoff) del filtro.
- Inclinar/mover en eje **Y** → resonancia (Q).

---

## 7. Si algo falla

| Síntoma | Solución |
|---|---|
| No hay audio / ruido | Verifica **Flash Mode = DIO** (OPI rompe el I2S). |
| Glitches de audio | Sube `LED_REFRESH_MS` (refresca los LEDs menos seguido). |
| Colores de LED raros | Cambia `COLOR_ORDER` de `GRB` a `RGB`. |
| LEDs muy brillantes/tenues | Ajusta `LED_BRIGHT` (0–255, default 50). |
| Pot no responde al cambiar de panel | Es normal: muévelo y retoma el control. |
