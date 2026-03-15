// ============================================================
//  PercuSynth — test_leds.ino
//  GC Lab Chile
//
//  Prueba de tira LED WS2812 conectada al pin 46.
//  LEDs 0-6 (SMD internos) siempre apagados.
//  LEDs 7-150 activos con 6 modos de animación.
//
//  Librería requerida: FastLED (instalar desde el gestor de librerías)
//
//  CONTROLES:
//    POT1 (ADC 1)  → Brillo
//    POT2 (ADC 2)  → Color (tono/hue)
//    POT3 (ADC 8)  → Velocidad de animación
//    POT4 (ADC 10) → Parámetro extra (varía según modo)
//
//    BTN1 (pin 42) → Modo siguiente
//    BTN2 (pin 44) → Modo anterior
//    BTN3 (pin 45) → Invertir dirección
//    BTN4 (pin 47) → Flash blanco
//    BTN5 (pin  0) → Apagar todo
// ============================================================

#include <FastLED.h>

// ─── Configuración de hardware ───────────────────────────────
#define LED_PIN      46
#define NUM_LEDS    150
#define START_LED     6    // LEDs 0-5 son SMD internos (montados al revés en v1) → siempre apagados
#define LED_TYPE     WS2812
#define COLOR_ORDER  GRB
#define MAX_BRIGHT   60    // 0-255 — mantener bajo para no sobrecargar la fuente

const uint8_t BTN_PINS[] = { 42, 44, 45, 47, 0 };
const uint8_t POT_PINS[] = {  1,  2,  8, 10 };
#define NUM_BTNS 5
#define NUM_POTS 4

// ─── Estado global ───────────────────────────────────────────
CRGB leds[NUM_LEDS];

uint8_t  modo       = 0;
bool     dirFwd     = true;
bool     apagado    = false;

// Valores leídos de pots
uint8_t  brillo     = 40;
uint8_t  hue        = 0;
uint16_t velocidad  = 40;   // ms por frame
uint8_t  param      = 128;  // parámetro extra

// Debounce botones
bool     btnEstado[NUM_BTNS]     = {};
bool     btnAnterior[NUM_BTNS]   = {};
uint32_t btnTiempo[NUM_BTNS]     = {};
#define  DEBOUNCE_MS 25

#define NUM_MODOS 6
const char* nombreModo[] = {
  "Solido", "Chase", "Rainbow", "Twinkle", "Pulso", "Meteor"
};

// ─── Utilidades ──────────────────────────────────────────────

uint16_t leerPot(uint8_t pin) {
  uint32_t suma = 0;
  for (int i = 0; i < 8; i++) suma += analogRead(pin);
  return suma >> 3;  // promedio 8 lecturas
}

// Devuelve true solo en el flanco de bajada (presión)
bool botonPresionado(uint8_t i) {
  bool lectura = (digitalRead(BTN_PINS[i]) == LOW);
  if (lectura != btnAnterior[i]) btnTiempo[i] = millis();
  btnAnterior[i] = lectura;
  if ((millis() - btnTiempo[i]) > DEBOUNCE_MS) {
    if (lectura && !btnEstado[i]) { btnEstado[i] = true;  return true; }
    if (!lectura)                   btnEstado[i] = false;
  }
  return false;
}

// Mantiene los SMD internos siempre apagados
void apagarInternos() {
  for (int i = 0; i < START_LED; i++) leds[i] = CRGB::Black;
}

// ─── Modos de animación ──────────────────────────────────────

// Modo 0 — Sólido
//   POT4 → saturación (0 = blanco, 255 = color puro)
void animSolido() {
  CRGB color = CHSV(hue, 255 - param, 255);
  fill_solid(leds + START_LED, NUM_LEDS - START_LED, color);
}

// Modo 1 — Chase
//   POT4 → largo de la cola (1-30 LEDs)
uint8_t chasePos = 0;
void animChase() {
  fadeToBlackBy(leds + START_LED, NUM_LEDS - START_LED, 60);
  uint8_t cola = map(param, 0, 255, 1, 30);
  int total = NUM_LEDS - START_LED;
  int pos = dirFwd ? chasePos : (total - 1 - chasePos);
  leds[START_LED + pos] = CHSV(hue, 255, 255);
  for (uint8_t t = 1; t <= cola; t++) {
    int tp = dirFwd ? (int)chasePos - t : (int)chasePos + t;
    if (tp >= 0 && tp < total)
      leds[START_LED + tp] = CHSV(hue, 255, 255 / (t + 1));
  }
  chasePos = (chasePos + 1) % total;
}

// Modo 2 — Rainbow
//   POT4 → densidad del arcoíris (qué tan comprimido)
uint8_t rainbowOffset = 0;
void animRainbow() {
  uint8_t delta = map(param, 0, 255, 1, 8);
  int signo = dirFwd ? 1 : -1;
  for (int i = START_LED; i < NUM_LEDS; i++)
    leds[i] = CHSV(rainbowOffset + signo * (i - START_LED) * delta, 255, 255);
  rainbowOffset++;
}

// Modo 3 — Twinkle
//   POT4 → cantidad de destellos simultáneos
void animTwinkle() {
  fadeToBlackBy(leds + START_LED, NUM_LEDS - START_LED, 25);
  uint8_t destellos = map(param, 0, 255, 1, 12);
  for (int i = 0; i < destellos; i++) {
    int pos = random16(NUM_LEDS - START_LED);
    leds[START_LED + pos] = CHSV(hue + random8(60), 200, 255);
  }
}

// Modo 4 — Pulso (breathing)
//   POT4 → saturación
uint8_t breathVal = 0;
int8_t  breathDir = 1;
void animPulso() {
  fill_solid(leds + START_LED, NUM_LEDS - START_LED,
             CHSV(hue, 255 - param, breathVal));
  breathVal += breathDir * 4;
  if (breathVal >= 252) breathDir = -1;
  if (breathVal <= 3)   breathDir =  1;
}

// Modo 5 — Meteor
//   POT4 → largo de la estela (3-40 LEDs)
uint8_t meteorPos = 0;
void animMeteor() {
  for (int i = START_LED; i < NUM_LEDS; i++)
    leds[i].fadeToBlackBy(48);
  uint8_t estela = map(param, 0, 255, 3, 40);
  int total = NUM_LEDS - START_LED;
  int pos = dirFwd ? meteorPos : (total - 1 - meteorPos);
  for (uint8_t t = 0; t < estela; t++) {
    int tp = dirFwd ? pos - t : pos + t;
    if (tp >= 0 && tp < total)
      leds[START_LED + tp] = CHSV(hue, 255, 255 - t * (255 / estela));
  }
  meteorPos = (meteorPos + 1) % total;
}

// ─── Setup ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  analogSetAttenuation(ADC_11db);  // rango completo 0-3.3V en los ADC

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(MAX_BRIGHT);
  FastLED.clear();
  FastLED.show();

  for (int i = 0; i < NUM_BTNS; i++)
    pinMode(BTN_PINS[i], INPUT_PULLUP);

  Serial.println("=== PercuSynth — Test LEDs WS2812 ===");
  Serial.println("Pin datos: 46 | LEDs activos: 7-150");
  Serial.println("BTN1/BTN2: cambiar modo | BTN3: dirección | BTN4: flash | BTN5: apagar");
  Serial.print("Modo inicial: "); Serial.println(nombreModo[modo]);
}

// ─── Loop ────────────────────────────────────────────────────
void loop() {
  uint32_t ahora = millis();
  static uint32_t ultimoFrame = 0;

  // ── Leer potenciómetros ──────────────────────────────────
  brillo    = map(leerPot(POT_PINS[0]), 0, 4095, 5, MAX_BRIGHT);
  hue       = map(leerPot(POT_PINS[1]), 0, 4095, 0, 255);
  velocidad = map(leerPot(POT_PINS[2]), 0, 4095, 5, 150);
  param     = map(leerPot(POT_PINS[3]), 0, 4095, 0, 255);
  FastLED.setBrightness(brillo);

  // ── Leer botones ─────────────────────────────────────────
  if (botonPresionado(0)) {  // BTN1 (42) → modo siguiente
    modo = (modo + 1) % NUM_MODOS;
    FastLED.clear();
    Serial.print("Modo: "); Serial.println(nombreModo[modo]);
  }
  if (botonPresionado(1)) {  // BTN2 (44) → modo anterior
    modo = (modo + NUM_MODOS - 1) % NUM_MODOS;
    FastLED.clear();
    Serial.print("Modo: "); Serial.println(nombreModo[modo]);
  }
  if (botonPresionado(2)) {  // BTN3 (45) → invertir dirección
    dirFwd = !dirFwd;
    Serial.print("Dirección: "); Serial.println(dirFwd ? "adelante" : "atrás");
  }
  if (botonPresionado(3)) {  // BTN4 (47) → flash blanco
    fill_solid(leds + START_LED, NUM_LEDS - START_LED, CRGB::White);
    apagarInternos();
    FastLED.show();
    delay(80);
    FastLED.clear();
  }
  if (botonPresionado(4)) {  // BTN5 (0) → apagar / reanudar (toggle)
    apagado = !apagado;
    if (apagado) {
      FastLED.clear();
      FastLED.show();
      Serial.println("Apagado");
    } else {
      Serial.println("Reanudado");
    }
  }

  // ── Renderizar frame ─────────────────────────────────────
  if (apagado) return;  // no renderizar mientras esté apagado

  if (ahora - ultimoFrame >= velocidad) {
    ultimoFrame = ahora;
    apagarInternos();

    switch (modo) {
      case 0: animSolido();  break;
      case 1: animChase();   break;
      case 2: animRainbow(); break;
      case 3: animTwinkle(); break;
      case 4: animPulso();   break;
      case 5: animMeteor();  break;
    }

    FastLED.show();
  }
}
