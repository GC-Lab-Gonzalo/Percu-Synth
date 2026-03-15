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
const uint8_t buttonNotes[NUM_BUTTONS] = {36, 38, 42, 46, 51};
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
      MIDI.noteOn(imu.notaPendiente, velocidad, 9);

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