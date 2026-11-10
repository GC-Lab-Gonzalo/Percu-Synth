# industrial_matriz_v2_triphop

Copia de `industrial_matriz_v2` con **un solo cambio: el segundo estilo, que era TRANCE, ahora es
TRIP HOP**. Los otros cuatro estilos (INDUSTRIAL · TECHNO · SYNTHWAVE · FUTURISMO), los paneles, la
matriz y la línea para Resonancia están idénticos. El original no se tocó.

El trip hop viene de `triphop_viaje`, la máquina dedicada al género (su dialecto BRISTOL):

- **Tempo y swing propios.** Con el trip hop el POT2 del General va de **58 a 110 BPM** (no 70–170)
  y el POT3 de **54 a 72 % de swing** (no recto–70 %). Al cambiar de estilo el tempo salta a su
  rango con el pot donde está: la misma posición que da 128 BPM en techno da 88 en trip hop.
- **Arrastre por pista.** La caja entra ~22 ms tarde, el clap ~26, el rim ~10 y el tom ~12, con una
  pizca de variación humana (±12 %). **El bombo nunca se atrasa**: es el tiempo. A 88 BPM una
  semicorchea son 170 ms, así que el arrastre no cabe en la rejilla: es un planificador aparte que
  dispara cada golpe en su muestra exacta (el tramo de render se corta en el próximo paso o en el
  próximo golpe pendiente). En los otros cuatro estilos el arrastre es cero y la batería suena igual
  que en el original.
- **Armonía:** Fa# **menor armónica**, i · VI · iv · V (F#m · D · Bm · C#), dos compases cada uno.
  El V mayor con la sensible es el color de cine negro del género. Todo sigue escrito en grados: nada
  sale de la escala.
- **Timbres que trae:** kit **POLVO** (nuevo; el de triphop_viaje: bombo blando, caja grande y oscura,
  rim, hats gastados), bajo **SUB**, melodía **VIDRIO**, oscilador **SUAVE**. Patrones del dialecto
  BRISTOL: 4 de batería (pesado · quebrado · medio tiempo · boom bap), 4 de bajo con notas largas y 4
  melodías con mucho espacio.
- **No vinieron**, a propósito, la capa de vinilo ni la de voz de triphop_viaje: las dos fueron
  rechazadas allá.

## Controles

Los del original, con dos diferencias: el POT2/POT3 del General cambian de rango con el trip hop, y
el BTN3 de la batería recorre **6 kits** (se sumó POLVO al final).

| Panel | BTN1 | BTN2 | BTN3 | POT1 | POT2 | POT3 | POT4 |
|---|---|---|---|---|---|---|---|
| General | play/stop | estilo | fill al final del compás | volumen | tempo 70–170 (trip hop 58–110) | swing (trip hop 54–72 %) | pasa-bajos general |
| Bajo | suena/calla | patrón | tipo (SIERRAS · SUB · ÁCIDO · REESE) | volumen | filtro | ataque | caída |
| Batería | suena/calla | patrón | kit (INDUSTRIAL · NEXO · 909 · 808 · GLITCH · POLVO) | volumen | filtro | largo de los golpes | densidad |
| Melodías | suena/calla | patrón | instrumento (KEYS · PLUCK · LEAD · VIDRIO) | volumen | filtro | ataque | caída |
| Oscilador | on/off | timbre | octava (normal · +1 · −1) | **nota** | filtro | LFO | envío al delay |

En todos los paneles: BTN4 = siguiente panel, BTN5 = break (mantenido 2 s: subida; el próximo
toque es el drop). El detalle completo está en el README de `industrial_matriz_v2` y en la cabecera
del `.ino`.

## Cargar

ESP32S3 Dev Module · **Flash Mode DIO** · USB CDC On Boot Enabled · PSRAM OPI. Requiere FastLED.

```
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=dio,PSRAM=opi" --upload -p COM43 .
```

## Pendiente

- Medir el CPU en la placa con `-DMEDIR=1` (el arrastre agrega cortes de tramo, no voces: debería
  quedar como el original, 61 % / 87 %).
