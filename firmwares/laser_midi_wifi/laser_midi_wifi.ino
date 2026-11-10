// ==============================================================================================================================================
// LASER MIDI WIFI — Prueba de enlace: una nota MIDI por segundo — GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: GC Lab Chile
// ==============================================================================================================================================
// REPOSITORIO: https://github.com/GC-Lab-Gonzalo/Percu-Synth
// ==============================================================================================================================================
// HARDWARE (usado por este firmware)
// ==============================================================================================================================================
// - Microcontrolador ESP32-C3 (por ejemplo una SuperMini). NO es un PercuSynth: es el nodo
//   sensor que le va a hablar a la música.
// - LED integrado de la placa |LED -> 8| (en la SuperMini se enciende en LOW). Si tu placa lo
//   tiene en otro pin, pon LED_ACTIVO en false: la prueba funciona igual.
// - Sin botones, sin perillas, sin audio y todavía sin sensor.
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32C3 Dev Module
// - USB CDC On Boot    : Enabled     <- sin esto no ves nada por el monitor serie
// - Upload Speed       : 921600
// - Monitor Serie      : 115200 baudios
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - "Arduino AppleMIDI Library" de lathoub: Gestor de librerías -> buscar "AppleMIDI" ->
//   instalar. Arrastra sola la librería MIDI de FortySevenEffects.
// - WiFi.h y ESPmDNS.h (incluidas en el core ESP32)
// ==============================================================================================================================================
// CREDENCIALES
// ==============================================================================================================================================
// No viven en este archivo. Copia secretos.example.h a secretos.h (misma
// carpeta del sketch) y escribe ahí el nombre y la clave de la red que crea el
// ESP32, y los de tu WiFi si vas a usar el modo cliente. secretos.h está en el
// .gitignore: nunca se sube al repo. Si falta, el sketch no compila y te dice qué hacer.
// - AP_SSID / AP_CLAVE    : la red que CREA el ESP32. La inventas tú; la clave tiene que
//                           tener 8 caracteres o más, si no la red no levanta.
// - WIFI_SSID / WIFI_CLAVE: tu red de siempre (modo cliente). Tiene que ser de 2.4 GHz.
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Prueba de enlace para un ESP32-C3 (no es un PercuSynth): manda un Do (nota MIDI 48) cada
// segundo por WiFi usando RTP-MIDI, el "MIDI de red" que Mac, iPhone y Windows ya entienden.
// No tiene botones ni perillas: no hay nada que tocar, solo conectarse y escuchar que llega.
//
// Comprueba que la cadena completa funciona antes de ponerle un sensor:
//   ESP32-C3  ->  WiFi  ->  sesión RTP-MIDI  ->  computador
// Cuando esto suene, el sensor es lo único que falta y lo demás ya está probado.
//
// DE DÓNDE VIENE
// Nació experimentando con malabarismo: la idea es detectar las clavas en el
// aire y que sus pasos generen música. El nodo sensor va lejos del computador
// (en el cuerpo, en el espacio donde se lanza), por eso el MIDI viaja por WiFi.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// Esta placa no tiene botones ni perillas: no hay nada que tocar. Todo se ajusta en el
// código antes de cargarlo.
// - Nota       → un Do (48), canal 1, velocidad 100, cada 1 s y sostenido 200 ms.
// - LED (8)    → se enciende con cada nota.
// - MODO_AP    → true (por defecto): el ESP32 crea su propia red · false: se une a tu WiFi.
// - Nombre     → se anuncia en la red como "GCLab-Laser" (NOMBRE_SESION).
// - Monitor    → muestra la IP y el puerto, y una línea por nota con "sesión: SI/no".
//
// Sobre MODO_AP: el punto de acceso propio es el modo para tocar (red vacía, un salto
// menos, no dependes del WiFi del lugar; el computador queda sin internet mientras está
// conectado). El modo cliente es más cómodo para desarrollar. En una casa tranquila la
// diferencia de latencia es de pocos milisegundos; el punto de acceso gana de verdad en
// lugares con WiFi saturado. El detalle está junto a la línea MODO_AP, más abajo.
//
// MODO DE USO:
// 1. Copia secretos.example.h a secretos.h y escribe ahí el nombre y la clave de la red.
// 2. Carga el sketch y abre el monitor serie a 115200: te muestra la dirección y el puerto.
// 3. Conecta el computador a la red del ESP32 ("sin internet" es lo esperado).
// 4. Abre la app que ABRE la sesión de red (ver la lista abajo) y conéctate a "GCLab-Laser".
// 5. Elige esa entrada MIDI en cualquier sinte o DAW: debería sonar un Do cada segundo.
// ==============================================================================================================================================
// APPS QUE ABREN LA SESIÓN (paso 4)
// ==============================================================================================================================================
// No cualquier app sirve:
//    - iPhone/iPad : "RTP-MIDI (Network MIDI)" o "NetMIDI" en la App Store.
//                    Una vez conectada, la sesión queda disponible para el
//                    resto de apps CoreMIDI del teléfono (Synth One la ve sola).
//                    OJO: midimittr NO sirve, solo hace Bluetooth.
//    - Windows     : rtpMIDI de Tobias Erichsen.
//    - Mac         : Audio MIDI Setup -> ventana Red.
// Busca "GCLab-Laser" (NOMBRE_SESION, más abajo) y dale Conectar.
// Synth One NO abre sesiones: solo recibe MIDI que ya exista.
// ==============================================================================================================================================
// SI NO APARECE EN LA LISTA
// ==============================================================================================================================================
// - En modo punto de acceso: confirma que el computador quedó realmente en la
//   red del ESP32 y no volvió solo a la de siempre. Pasa seguido, porque los
//   sistemas prefieren las redes con internet.
// - En modo cliente: el ESP32 y el computador tienen que estar en la misma red
//   y en la misma banda. Los routers que separan 2.4 y 5 GHz en redes distintas
//   rompen el descubrimiento. Y algunos bloquean multicast entre clientes.
// - En cualquier caso, en Mac se puede agregar a mano por IP con el botón "+"
//   del directorio, y la sesión funciona igual.
// - El monitor serie separa dos problemas que se confunden: "en el WiFi: 1" dice que el
//   computador está en la red del ESP32; "sesión: SI" dice que además la sesión MIDI se abrió.
// ==============================================================================================================================================
// OJO CON EL AHORRO DE ENERGÍA
// ==============================================================================================================================================
// La línea WiFi.setSleep(false) de más abajo NO es opcional. Sin ella el ESP32
// duerme entre paquetes y mete decenas de milisegundos de jitter impredecible
// — justo lo que fuimos a buscar el WiFi para evitar. Si algún día notas el
// timing errático, lo primero que hay que mirar es esa línea.
// ==============================================================================================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AppleMIDI.h>

#if !__has_include("secretos.h")
  #error "Falta secretos.h: copia secretos.example.h a secretos.h y pon el nombre y la clave de las redes."
#endif
#include "secretos.h"     // AP_SSID, AP_CLAVE, WIFI_SSID, WIFI_CLAVE

// --- WiFi: dos modos, elige uno ---------------------------------------------
//
// MODO_AP = true   -> el ESP32 CREA su propia red y el computador se conecta a
//                     ella. Es el modo para tocar. Red vacía, un salto menos
//                     (no pasa por ningún router) y no dependes de la clave del
//                     lugar donde estés. A cambio, el computador se queda sin
//                     internet mientras esté conectado acá.
//
// MODO_AP = false  -> el ESP32 se une a tu red de siempre. Más cómodo para
//                     desarrollar, porque no tienes que cambiar de red cada vez
//                     que quieras buscar algo.
//
// SOBRE LA LATENCIA, SIN EXAGERAR: en una red de casa tranquila la diferencia
// entre los dos modos es de pocos milisegundos. Donde el punto de acceso propio
// gana de verdad es en un lugar con WiFi saturado —un evento, una sala con
// veinte celulares— porque ahí la red compartida te mete jitter impredecible.
// Para tocar en vivo, punto de acceso propio. Para probar en la mesa, da igual.
//
const bool MODO_AP = true;
// Los nombres y claves de las dos redes están en secretos.h (ver CREDENCIALES arriba).

// --- Identidad en la red ----------------------------------------------------
// SIN ESPACIOS NI TILDES. Este nombre se usa como nombre de host en mDNS, y los
// nombres de host no admiten espacios: si los pones, el anuncio en la red falla
// y el aparato no aparece en la lista de ninguna app. Por eso el nombre por
// defecto de la librería es "AppleMIDI-ESP32" y no algo con espacios.
#define NOMBRE_SESION "GCLab-Laser"

// --- Qué toca ---------------------------------------------------------------
const uint8_t  CANAL_MIDI   = 1;
const uint8_t  NOTA         = 48;    // Do central de la octava 3
const uint8_t  VELOCIDAD    = 100;
const uint32_t INTERVALO_MS = 1000;  // una nota por segundo
const uint32_t DURACION_MS  = 200;   // cuánto se mantiene apretada

// --- LED de vida ------------------------------------------------------------
// El pin del LED integrado cambia según la placa C3: en la SuperMini es el 8 y
// se enciende en LOW. Si tu placa no responde, pon LED_ACTIVO en false y guíate
// por el monitor serie: la prueba funciona igual.
const bool    LED_ACTIVO   = true;
const uint8_t LED_PIN      = 8;
const bool    LED_INVERTIDO = true;

// ----------------------------------------------------------------------------

// Instancia con nombre propio. El macro por defecto la llamaría "AppleMIDI-ESP32".
// Crea dos objetos: AppleMIDI (la sesión) y MIDI (por donde se manda y recibe).
APPLEMIDI_CREATE_INSTANCE(WiFiUDP, MIDI, NOMBRE_SESION, DEFAULT_CONTROL_PORT);

uint32_t ultimaNota   = 0;
bool     notaSonando  = false;
bool     sesionActiva = false;

void encenderLed(bool on) {
  if (!LED_ACTIVO) return;
  digitalWrite(LED_PIN, LED_INVERTIDO ? !on : on);
}

void levantarRedPropia() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_CLAVE);

  // En modo punto de acceso la radio no entra en ahorro de energía, pero se
  // deja igual por si algún día se cambia a modo mixto.
  WiFi.setSleep(false);

  Serial.println();
  Serial.printf("Red creada: %s   (la clave está en secretos.h)\n", AP_SSID);
  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Conecta el computador a esa red antes de buscar la sesión MIDI.");
}

void unirseARed() {
  Serial.printf("Conectando a %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_CLAVE);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  // Lo más importante del sketch: ver el comentario del encabezado.
  WiFi.setSleep(false);

  Serial.println();
  Serial.print("Conectado. IP: ");
  Serial.println(WiFi.localIP());
}

void conectarWifi() {
  if (MODO_AP) levantarRedPropia();
  else         unirseARed();
}

void setup() {
  Serial.begin(115200);
  delay(1500);                  // margen para que el USB CDC levante y no te
                                // pierdas las primeras líneas

  if (LED_ACTIVO) {
    pinMode(LED_PIN, OUTPUT);
    encenderLed(false);
  }

  Serial.println();
  Serial.println("=== GC Lab — prueba de RTP-MIDI por WiFi ===");

  conectarWifi();

  MIDI.begin();

  // Se anuncia EXACTAMENTE el nombre y el puerto que reporta la librería, no
  // constantes propias que puedan quedar desincronizadas.
  if (!MDNS.begin(AppleMIDI.getName())) {
    Serial.println("AVISO: mDNS no arrancó. Hay que conectarse a mano por IP.");
  } else {
    MDNS.addService("apple-midi", "udp", AppleMIDI.getPort());
  }

  // Estos tres avisos son cómodos pero no imprescindibles. Si tu versión de la
  // librería reclama por la firma de las funciones, bórralos: la prueba corre
  // igual y el monitor serie te sigue mostrando cada nota enviada.
  AppleMIDI.setHandleConnected([](const APPLEMIDI_NAMESPACE::ssrc_t& ssrc, const char* nombre) {
    sesionActiva = true;
    Serial.printf("Sesión abierta con: %s\n", nombre);
  });
  AppleMIDI.setHandleDisconnected([](const APPLEMIDI_NAMESPACE::ssrc_t& ssrc) {
    sesionActiva = false;
    Serial.println("Sesión cerrada.");
  });

  Serial.println();
  Serial.println("--- DATOS PARA CONECTARSE ---");
  Serial.printf("Se anuncia como : %s\n", NOMBRE_SESION);
  Serial.printf("Dirección       : %s\n",
                MODO_AP ? WiFi.softAPIP().toString().c_str()
                        : WiFi.localIP().toString().c_str());
  Serial.printf("Puerto          : %u\n", AppleMIDI.getPort());
  Serial.println();
  Serial.println("Necesitas una app que ABRA la sesión de red:");
  Serial.println("  iPhone : \"RTP-MIDI (Network MIDI)\" o \"NetMIDI\"");
  Serial.println("  Windows: rtpMIDI de Tobias Erichsen");
  Serial.println("  Mac    : Audio MIDI Setup -> ventana Red");
  Serial.println("Synth One NO abre sesiones: solo recibe MIDI que ya exista.");
  Serial.println("midimittr tampoco sirve: es solo Bluetooth.");
  Serial.println("Si el aparato no sale en la lista, conéctate a mano con la");
  Serial.println("dirección y el puerto de arriba.");
  Serial.println("-----------------------------");

  ultimaNota = millis();
}

void loop() {
  // Se llama SIEMPRE y sin bloquear: es lo que mantiene viva la sincronización
  // de reloj de la sesión. Nunca metas delay() largos en este loop.
  MIDI.read();

  const uint32_t ahora = millis();

  if (!notaSonando && (ahora - ultimaNota >= INTERVALO_MS)) {
    MIDI.sendNoteOn(NOTA, VELOCIDAD, CANAL_MIDI);
    notaSonando = true;
    ultimaNota  = ahora;
    encenderLed(true);
    // El contador de clientes separa dos problemas que se confunden: estar en
    // la red del ESP32 no es lo mismo que tener sesión MIDI abierta.
    if (MODO_AP) {
      Serial.printf("Note On  %u  |  sesión: %s  |  en el WiFi: %u\n", NOTA,
                    sesionActiva ? "SI" : "no",
                    WiFi.softAPgetStationNum());
    } else {
      Serial.printf("Note On  %u  |  sesión: %s\n", NOTA,
                    sesionActiva ? "SI" : "no");
    }
  }

  if (notaSonando && (ahora - ultimaNota >= DURACION_MS)) {
    MIDI.sendNoteOff(NOTA, 0, CANAL_MIDI);
    notaSonando = false;
    encenderLed(false);
  }

  // Si el WiFi se cae, reconectar sin dejar la nota apretada para siempre.
  // En modo punto de acceso no aplica: la red es el propio ESP32 y no se "cae".
  if (!MODO_AP && WiFi.status() != WL_CONNECTED) {
    if (notaSonando) {
      MIDI.sendNoteOff(NOTA, 0, CANAL_MIDI);
      notaSonando = false;
      encenderLed(false);
    }
    Serial.println("WiFi caído, reconectando...");
    unirseARed();
  }
}
