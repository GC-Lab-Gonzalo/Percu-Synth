// ==============================================================================================================================================
// PERCU-SYNTH — MIDI Drum (controlador MIDI USB) — GC Lab Chile
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
// - Microcontrolador ESP32-S3 (USB nativo)
// - 5 Botones con pull-up |BTN1 -> 42, BTN2 -> 44, BTN3 -> 45, BTN4 -> 47, BTN5 -> 0|
// - 4 Sensores piezoeléctricos |P1 -> ADC4, P2 -> ADC5, P3 -> ADC6, P4 -> ADC7|
// - Sensor de movimiento IMU MPU6050 (I2C) |VCC -> 3.3V, GND -> GND, SDA -> 21, SCL -> 38|
// - Salida MIDI USB nativa (canal 10 / "9" en notación 0-indexed = GM Drums)
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - USB Mode           : USB-OTG (TinyUSB)
// - USB DFU On Boot    : Disabled
// - Flash Mode         : DIO
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core ≥ 3.x (USB.h, USBMIDI.h)
// - Wire.h (I2C — incluido con el core)
// - MPU6050 — Electronic Cats / Jeff Rowberg (gestor de librerías Arduino)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Controlador MIDI de percusión. No genera audio: traduce golpes físicos y
// gestos en mensajes MIDI USB que se envían a un DAW (Ableton, GarageBand,
// FL Studio, Logic, etc.) o a un sintetizador externo por USB-MIDI.
//
// El truco "groove": cada vez que se aprieta un botón, el firmware abre una
// ventana de 20 ms y muestrea el acelerómetro. La intensidad del movimiento
// del PercuSynth en el aire determina la VELOCIDAD MIDI (40–127). Así los
// botones suenan distinto según con cuánta fuerza muevas el dispositivo.
// Los piezos en cambio derivan la velocidad de la amplitud real del golpe.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// CONTROLES → SALIDA MIDI (todos en canal 10 = GM Drums)
// - BTN1 (42) → nota 36 (KICK)        — velocidad por IMU
// - BTN2 (44) → nota 38 (SNARE)       — velocidad por IMU
// - BTN3 (45) → nota 46 (HI-HAT abierto) — velocidad por IMU
// - BTN4 (47) → nota 51 (RIDE)        — velocidad por IMU
// - BTN5 (0)  → nota 42 (HI-HAT cerrado) — velocidad por IMU
//
// - PIEZO1 (ADC4) → nota 49 "CRASH"        — velocidad por amplitud del golpe
// - PIEZO2 (ADC5) → nota 45 "TOM 1"        — velocidad por amplitud del golpe
// - PIEZO3 (ADC6) → nota 47 "TOM 2"        — velocidad por amplitud del golpe
// - PIEZO4 (ADC7) → nota 43 "FLOOR TOM"    — velocidad por amplitud del golpe
//
// PARÁMETROS DE TIEMPO:
// - Ventana piezo:        15 ms (peak detection)
// - Ventana IMU botón:    20 ms (acceleration peak)
// - Anti-retrigger piezo: 50 ms
// - NoteOff piezo:        30 ms después del NoteOn
//
// MODO DE USO:
// 1. Conectar el PercuSynth al PC/Mac por USB-C → aparece como "ESP32-S3 MIDI".
// 2. Abrir el DAW y seleccionar el dispositivo como entrada MIDI (canal 10).
// 3. Tocar los botones (mover el aparato → más fuerza = más velocity) o golpear
//    los piezos para disparar las pads de percusión del DAW.
// ==============================================================================================================================================

#include "USB.h"
#include "USBMIDI.h"
#include <Wire.h>
#include <MPU6050.h>

USBMIDI MIDI;
MPU6050 mpu;

#define IMU_SDA 21
#define IMU_SCL 38

// ---- BOTONES ----
#define NUM_BUTTONS 5
const int buttonPins[NUM_BUTTONS]      = {42, 44, 45, 47, 0};
const uint8_t buttonNotes[NUM_BUTTONS] = {36, 38, 46, 51, 42};
bool lastState[NUM_BUTTONS];

// ---- COLA DE BOTONES ----
#define QUEUE_SIZE 5
struct BtnQueue {
  uint8_t nota;
  int     idx;
};
BtnQueue btnQueue[QUEUE_SIZE];
int qHead = 0;
int qTail = 0;

void encolarBoton(int idx) {
  int nextTail = (qTail + 1) % QUEUE_SIZE;
  if (nextTail != qHead) { // cola no llena
    btnQueue[qTail] = {buttonNotes[idx], idx};
    qTail = nextTail;
  }
}

// ---- PIEZOS ----
#define NUM_PIEZOS 4
const int piezoPins[NUM_PIEZOS]        = {4, 5, 6, 7};
const uint8_t piezoNotes[NUM_PIEZOS]   = {49, 45, 47, 43};
const char* piezoNames[NUM_PIEZOS]     = {"CRASH","TOM1","TOM2","FLOOR TOM"};

const int UMBRAL_PIEZO = 300;
const int RETRIGGER_MS = 50;
const int VENTANA_MS   = 15;

struct PiezoState {
  bool          muestreando;
  bool          noteOnActivo;
  int           maxVal;
  int           minVal;
  unsigned long ventanaInicio;
  unsigned long lastHit;
  unsigned long noteOnTime;
};
PiezoState ps[NUM_PIEZOS];

// ---- IMU ----
const float ACCEL_MIN = 0.04;
const float ACCEL_MAX = 0.35;

struct IMUState {
  bool          muestreando;
  float         maxMag;
  unsigned long ventanaInicio;
  int           botonPendiente;
  uint8_t       notaPendiente;
};
IMUState imu = {false, 0, 0, -1, 0};

float accelMag() {
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  float x = ax / 16384.0;
  float y = ay / 16384.0;
  float z = (az / 16384.0) - 1.0;
  return sqrt(x*x + y*y + z*z);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(IMU_SDA, IMU_SCL);
  mpu.initialize();
  Serial.println(mpu.testConnection() ? "MPU6050 OK" : "MPU6050 ERROR");

  MIDI.begin();
  USB.begin();

  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    lastState[i] = HIGH;
  }

  for (int i = 0; i < NUM_PIEZOS; i++) {
    ps[i] = {false, false, 0, 4095, 0, 0, 0};
  }

  Serial.println("=== LISTO ===");
}

void loop() {

  unsigned long ahora = millis();

  // ---- BOTONES — encolar ----
  for (int i = 0; i < NUM_BUTTONS; i++) {
    bool cur = digitalRead(buttonPins[i]);

    if (cur == LOW && lastState[i] == HIGH) {
      encolarBoton(i);
    }
    if (cur == HIGH && lastState[i] == LOW) {
      MIDI.noteOff(buttonNotes[i], 0, 9);
    }
    lastState[i] = cur;
  }

  // ---- IMU — procesa cola no-bloqueante ----
  if (!imu.muestreando && qHead != qTail) {
    imu.muestreando    = true;
    imu.maxMag         = 0;
    imu.ventanaInicio  = ahora;
    imu.botonPendiente = btnQueue[qHead].idx;
    imu.notaPendiente  = btnQueue[qHead].nota;
    qHead = (qHead + 1) % QUEUE_SIZE;
  }

  if (imu.muestreando) {
    float mag = accelMag();
    if (mag > imu.maxMag) imu.maxMag = mag;

    if (ahora - imu.ventanaInicio >= 20) {
      int vel = map((int)(imu.maxMag * 100),
                    (int)(ACCEL_MIN * 100),
                    (int)(ACCEL_MAX * 100), 40, 127);
      uint8_t velocidad = constrain(vel, 40, 127);
      MIDI.noteOn(imu.notaPendiente, velocidad, 1);

      Serial.print("BTN pin ");
      Serial.print(buttonPins[imu.botonPendiente]);
      Serial.print(" | accel: ");
      Serial.print(imu.maxMag, 3);
      Serial.print(" g | vel: ");
      Serial.println(velocidad);

      imu.muestreando = false;
    }
  }

  // ---- PIEZOS no-bloqueantes ----
  for (int i = 0; i < NUM_PIEZOS; i++) {

    // Detectar inicio de golpe
    if (!ps[i].muestreando && (ahora - ps[i].lastHit > RETRIGGER_MS)) {
      int val = analogRead(piezoPins[i]);
      if (val > UMBRAL_PIEZO) {
        ps[i].muestreando   = true;
        ps[i].maxVal        = val;
        ps[i].minVal        = val;
        ps[i].ventanaInicio = ahora;
      }
    }

    // Acumular muestras durante ventana
    if (ps[i].muestreando) {
      int val = analogRead(piezoPins[i]);
      if (val > ps[i].maxVal) ps[i].maxVal = val;
      if (val < ps[i].minVal) ps[i].minVal = val;

      if (ahora - ps[i].ventanaInicio >= VENTANA_MS) {
        int amplitud      = ps[i].maxVal - ps[i].minVal;
        uint8_t velocidad = map(amplitud, UMBRAL_PIEZO, 3800, 40, 127);
        velocidad         = constrain(velocidad, 40, 127);

        MIDI.noteOn(piezoNotes[i], velocidad, 9);
        ps[i].noteOnActivo = true;
        ps[i].noteOnTime   = ahora;

        Serial.print("PIEZO ");
        Serial.print(piezoNames[i]);
        Serial.print(" | amplitud: ");
        Serial.print(amplitud);
        Serial.print(" | vel: ");
        Serial.println(velocidad);

        ps[i].lastHit     = ahora;
        ps[i].muestreando = false;
        ps[i].maxVal      = 0;
        ps[i].minVal      = 4095;
      }
    }

    // NoteOff 30ms después del noteOn
    if (ps[i].noteOnActivo && (ahora - ps[i].noteOnTime >= 30)) {
      MIDI.noteOff(piezoNotes[i], 0, 9);
      ps[i].noteOnActivo = false;
    }
  }
}