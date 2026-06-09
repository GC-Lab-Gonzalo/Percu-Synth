// ==============================================================================================================================================
// PERCU-SYNTH — Test IMU (MPU6050) — GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo - GC Lab Chile · MIT License
// ==============================================================================================================================================
// HARDWARE
// ==============================================================================================================================================
// - ESP32-S3 + IMU MPU6050 (I2C) |SDA -> 21, SCL -> 38, VCC -> 3.3V, GND -> GND|  (dir. 0x68)
// ==============================================================================================================================================
// ARDUINO IDE
// ==============================================================================================================================================
// - Board: ESP32S3 Dev Module · USB CDC On Boot: Enabled · Monitor Serie a 115200 baudios
// - Librerías: solo Wire.h (incluida en el core)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Comprobación mínima del IMU: verifica que responde (WHO_AM_I = 0x68) y muestra
// por el Monitor Serie la aceleración (g) y el giro (°/s) de los 3 ejes.
// Si ves los números cambiar al mover el equipo, el IMU funciona.
// ==============================================================================================================================================

#include <Wire.h>

#define SDA_PIN   21
#define SCL_PIN   38
#define IMU_ADDR  0x68

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  // WHO_AM_I (0x75) debe devolver 0x68
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x75);
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, 1, true);
  uint8_t who = Wire.available() ? Wire.read() : 0xFF;

  Serial.print("WHO_AM_I = 0x");
  Serial.println(who, HEX);
  if (who == 0x68) {
    Serial.println(">> IMU detectado correctamente. :)");
  } else {
    Serial.println(">> NO se detecta el IMU. Revisa cableado (SDA 21, SCL 38, 3.3V, GND).");
  }

  // Despertar el MPU6050 (PWR_MGMT_1 = 0)
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission(true);

  delay(100);
  Serial.println("Mueve el equipo y observa los valores:");
}

void loop() {
  // Leer 14 bytes desde ACCEL_XOUT_H: accel(6) + temp(2) + gyro(6)
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, 14, true);

  if (Wire.available() >= 14) {
    int16_t ax = (Wire.read() << 8) | Wire.read();
    int16_t ay = (Wire.read() << 8) | Wire.read();
    int16_t az = (Wire.read() << 8) | Wire.read();
    int16_t tp = (Wire.read() << 8) | Wire.read();   // temperatura
    int16_t gx = (Wire.read() << 8) | Wire.read();
    int16_t gy = (Wire.read() << 8) | Wire.read();
    int16_t gz = (Wire.read() << 8) | Wire.read();

    // Conversión a unidades reales (config por defecto: ±2g y ±250°/s)
    float AX = ax / 16384.0f, AY = ay / 16384.0f, AZ = az / 16384.0f;   // g
    float GX = gx / 131.0f,   GY = gy / 131.0f,   GZ = gz / 131.0f;     // °/s
    float TEMP = tp / 340.0f + 36.53f;                                  // °C

    Serial.print("ACC[g] x="); Serial.print(AX, 2);
    Serial.print(" y=");       Serial.print(AY, 2);
    Serial.print(" z=");       Serial.print(AZ, 2);
    Serial.print("  GIRO[deg/s] x="); Serial.print(GX, 1);
    Serial.print(" y=");              Serial.print(GY, 1);
    Serial.print(" z=");              Serial.print(GZ, 1);
    Serial.print("  T="); Serial.print(TEMP, 1); Serial.println("C");
  } else {
    Serial.println("Sin datos del IMU (revisa conexión).");
  }

  delay(200);   // ~5 lecturas por segundo
}
