// ==============================================================================================================================================
// PERCU-SYNTH — Test IMU con LEDs (sin USB / sin Serial) — GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo - GC Lab Chile
// Licencia de Software: MIT License (https://opensource.org/licenses/MIT)
// Licencia de Hardware: CERN Open Hardware Licence v2 - Permissive (CERN-OHL-P)
//
// Puedes usar, modificar y distribuir este código y hardware, siempre que se mantenga
// la atribución a GC Lab Chile. Se entrega "tal cual", sin garantías de ningún tipo.
// ==============================================================================================================================================
// REPOSITORIO: https://github.com/GC-Lab-Gonzalo/Percu-Synth
// ==============================================================================================================================================
// HARDWARE (usado por este firmware)
// ==============================================================================================================================================
// - Microcontrolador ESP32-S3
// - IMU MPU6050 (I2C) |SDA -> 21, SCL -> 38|  (dir. 0x68)
// - Tira WS2812 externa |DATA -> 46| (hasta 150 LEDs; los primeros 6 son SMD internos → se dejan apagados)
// - No usa botones, potenciómetros ni audio.
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - Flash Mode         : DIO
// - USB CDC On Boot    : indistinto (no usamos Serial)
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - FastLED (gestor de librerías de Arduino)
// - Wire.h (incluida en el core ESP32)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Comprobación del IMU SIN depender del USB ni del Monitor Serie (evita los reinicios
// por el CDC). El resultado se ve en la tira de LED:
//   · IMU NO detectado  → toda la tira parpadea en ROJO.
//   · IMU OK            → barra VERDE de "nivel de movimiento": en reposo se ven
//                         pocos LEDs; al mover / girar el equipo, la barra crece y
//                         vira a amarillo/rojo. Si la barra reacciona al moverlo,
//                         el IMU funciona. :)
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// - BTN1 → sin uso
// - BTN2 → sin uso
// - BTN3 → sin uso
// - BTN4 → sin uso
// - BTN5 → sin uso
// - POT1 → sin uso
// - POT2 → sin uso
// - POT3 → sin uso
// - POT4 → sin uso
// - IMU → Gira el equipo: más rápido, más larga la barra de LEDs.
//
// La barra mide la velocidad de giro (giroscopio, los 3 ejes juntos): ~400 °/s la llena
// entera. Sube rápido y baja suave, como un vúmetro. En reposo siempre quedan 3 LEDs
// verdes encendidos: es la señal de que el IMU está vivo. Inclinar despacio casi no la
// mueve; lo que cuenta es girar.
//
// Los 6 LEDs SMD de la placa quedan apagados a propósito: el resultado SOLO se ve en la
// tira externa. Si no tienes tira conectada, no vas a ver nada.
//
// Ojo: este sketch exige WHO_AM_I = 0x68. Un MPU6050 clon que contesta otro valor (0x70,
// 0x72, 0x98…) también hace parpadear en rojo aunque funcione: compruébalo con test_imu.
//
// MODO DE USO:
// 1. Conecta la tira WS2812 externa al GPIO 46 y carga el sketch.
// 2. Mira la tira: si parpadea entera en ROJO, el IMU no responde (revisa el cableado: pines 21 y 38).
// 3. Si ves 3 LEDs verdes quietos, el IMU está OK.
// 4. Gira el equipo: la barra crece y pasa de verde a amarillo y rojo.
// ==============================================================================================================================================

#include <FastLED.h>
#include <Wire.h>

// ─── IMU ───────────────────────────────────────────────────
#define SDA_PIN   21
#define SCL_PIN   38
#define IMU_ADDR  0x68

// ─── Tira WS2812 ───────────────────────────────────────────
#define LED_PIN     46
#define NUM_LEDS   150
#define START_LED    6      // LEDs 0-5 son SMD internos → siempre apagados
#define MAX_BRIGHT  50

CRGB leds[NUM_LEDS];
bool imuOK = false;

// Suavizado del nivel y "pico que cae" para una barra agradable
float level = 0.0f;

// ─── IMU: init + chequeo WHO_AM_I ──────────────────────────
bool initIMU() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x75);                       // WHO_AM_I
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, 1, true);
  uint8_t who = Wire.available() ? Wire.read() : 0xFF;

  // Despertar
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);
  Wire.endTransmission(true);
  delay(50);

  return (who == 0x68);
}

// Lee el giroscopio y devuelve la magnitud (°/s) — responde a cualquier movimiento
float readGyroMag() {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x43);                       // GYRO_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, 6, true);
  if (Wire.available() >= 6) {
    int16_t gx = (Wire.read() << 8) | Wire.read();
    int16_t gy = (Wire.read() << 8) | Wire.read();
    int16_t gz = (Wire.read() << 8) | Wire.read();
    float x = gx / 131.0f, y = gy / 131.0f, z = gz / 131.0f;
    return sqrtf(x * x + y * y + z * z);
  }
  return 0.0f;
}

void setup() {
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(MAX_BRIGHT);
  FastLED.clear(true);

  imuOK = initIMU();
}

void loop() {
  if (!imuOK) {
    // IMU no detectado → parpadeo rojo
    static bool on = false;
    on = !on;
    fill_solid(leds, NUM_LEDS, on ? CRGB::Red : CRGB::Black);
    for (int i = 0; i < START_LED; i++) leds[i] = CRGB::Black;
    FastLED.show();
    delay(300);
    return;
  }

  // IMU OK → barra de nivel de movimiento
  float mag = readGyroMag();                 // °/s (≈0 en reposo, sube al mover)
  float target = mag / 400.0f;               // 400 °/s ≈ barra llena
  if (target > 1.0f) target = 1.0f;

  // Suavizado: sube rápido, baja suave (efecto VU)
  if (target > level) level += (target - level) * 0.6f;
  else                level += (target - level) * 0.08f;

  int usable = NUM_LEDS - START_LED;
  int lit = (int)(level * usable);
  int base = 3;                              // siempre algunos verdes = "IMU vivo"
  if (lit < base) lit = base;

  for (int i = 0; i < NUM_LEDS; i++) {
    if (i < START_LED) { leds[i] = CRGB::Black; continue; }
    int k = i - START_LED;
    if (k < lit) {
      // Gradiente verde → amarillo → rojo según la posición
      uint8_t hue = map(k, 0, usable - 1, 96, 0);   // 96=verde, 0=rojo
      leds[i] = CHSV(hue, 255, 255);
    } else {
      leds[i] = CRGB::Black;
    }
  }
  FastLED.show();
  delay(15);
}
