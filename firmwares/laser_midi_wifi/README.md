# laser_midi_wifi — Nodo sensor que manda MIDI por WiFi (prueba de enlace)

Este firmware nació experimentando con **malabarismo**: la idea es **detectar las clavas en el
aire y que sus pasos generen música**. El sensor tiene que ir lejos del computador (en el
espacio donde se lanza), así que el MIDI viaja **por WiFi** con **RTP-MIDI** (el "MIDI de red"
que Mac, iPhone y Windows ya entienden).

Por ahora **no hay sensor todavía**: el sketch manda **un Do cada segundo** para comprobar que la
cadena completa funciona:

```
ESP32-C3  →  WiFi  →  sesión RTP-MIDI  →  computador / iPhone
```

Cuando esto suena, lo único que falta es el sensor; lo demás ya está probado.

## Placa

**ESP32-C3** (una SuperMini, por ejemplo): **no es un PercuSynth**, es el nodo sensor que le va a
hablar a la música.

- Placa: **ESP32C3 Dev Module** · USB CDC On Boot: **Enabled** · Upload Speed: 921600
- Librería: **Arduino AppleMIDI Library** (lathoub), desde el gestor de librerías. Arrastra sola la
  librería MIDI de FortySevenEffects.

## Credenciales

El nombre y la clave de las redes **no van en el `.ino`**. Copia `secretos.example.h` como
`secretos.h` en esta misma carpeta y complétalo:

- `AP_SSID` / `AP_CLAVE`: la red que **crea el ESP32** (modo punto de acceso). La inventas tú; la
  clave tiene que tener **8 caracteres o más**.
- `WIFI_SSID` / `WIFI_CLAVE`: tu red de siempre, si usas el modo cliente. Tiene que ser de
  **2.4 GHz**.

`secretos.h` está en el `.gitignore`: nunca se sube. Si falta, el sketch no compila y te dice
qué hacer.

## Dos modos de red (`MODO_AP` en el `.ino`)

- **`true` (por defecto) — el ESP32 crea su propia red.** Es el modo para tocar: red vacía, un
  salto menos y no dependes del WiFi del lugar. El computador queda sin internet mientras está
  conectado ahí.
- **`false` — el ESP32 se une a tu red.** Más cómodo para desarrollar.

En una casa tranquila la diferencia de latencia es de pocos milisegundos; el punto de acceso
propio gana de verdad en lugares con WiFi saturado (un evento, una sala llena de celulares).

## Cómo probarlo

1. Completa `secretos.h`, carga el sketch y abre el monitor serie a 115200: te muestra la IP.
2. Conecta el computador a la red del ESP32 (va a decir "sin internet": es lo esperado).
3. Abre la app que **abre la sesión de red**: en iPhone/iPad "RTP-MIDI (Network MIDI)" o
   "NetMIDI"; en Windows **rtpMIDI** de Tobias Erichsen; en Mac **Configuración de Audio MIDI →
   ventana Red**. Busca **GCLab-Laser** y conéctate.
4. Elige esa entrada MIDI en cualquier sinte o DAW: debería sonar un Do cada segundo.

Ojo: **Synth One no abre sesiones** (sólo recibe MIDI que ya existe) y **midimittr sólo hace
Bluetooth**: ninguno de los dos sirve para el paso 3.

## Si no aparece en la lista

- En modo punto de acceso: revisa que el computador siga en la red del ESP32. Los sistemas
  tienden a volver solos a una red con internet.
- En modo cliente: el ESP32 y el computador deben estar en la misma red y la misma banda. Los
  routers que separan 2.4 y 5 GHz, o que bloquean el multicast entre clientes, rompen el
  descubrimiento.
- En Mac se puede agregar a mano por IP con el botón "+" del directorio.

## Decisiones

- **`WiFi.setSleep(false)` no es opcional.** Sin esa línea el ESP32 duerme entre paquetes y
  mete decenas de milisegundos de jitter — justo lo que se fue a buscar el WiFi para evitar.
- **El nombre de la sesión no lleva espacios ni tildes**: se usa como nombre de host en mDNS, y
  con espacios el anuncio falla y el aparato no aparece en ninguna app.
- `MIDI.read()` se llama siempre y sin bloquear: es lo que mantiene viva la sincronización de
  reloj de la sesión.
