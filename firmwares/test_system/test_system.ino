// ==============================================================================================================================================
// PERCU-SYNTH — Test SISTEMA (monitor de todos los componentes) — GC Lab Chile
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
// HARDWARE (todo el PercuSynth)
// ==============================================================================================================================================
// - Microcontrolador ESP32-S3
// - IMU MPU6050 (I2C)            |SDA -> 21, SCL -> 38, 3.3V, GND|  (dir. 0x68)
// - 5 Botones con pull-up         |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros (ADC 12-bit) |POT1 -> GPIO1, POT2 -> GPIO2, POT3 -> GPIO8, POT4 -> GPIO10|
// - 4 Sensores piezo (ADC)        |PZ1 -> GPIO4, PZ2 -> GPIO5, PZ3 -> GPIO6, PZ4 -> GPIO7|
// - 2 Sensores externos (ADC)     |EXT_A -> GPIO3, EXT_B -> GPIO9|
// - DAC PCM5102 vía I2S           |LCK -> 39, DIN -> 40, BCK -> 41|  (test de tono opcional)
// - Tira LED WS2812               |DATA -> 46|  (test de barrido opcional, requiere FastLED)
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - Flash Mode         : DIO   (importante para que el audio I2S funcione en este hardware)
// - Monitor Serie      : 115200 baudios
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - Wire.h            (incluida en el core ESP32)        → IMU
// - driver/i2s_std.h  (incluida en el core ESP32 >= 3.x) → test de audio
// - FastLED           (solo si activas ENABLE_LED_TEST)  → test de tira LED
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Firmware de diagnóstico: monitorea por el Monitor Serie el estado en vivo de TODOS
// los componentes del PercuSynth en una sola pantalla, y permite lanzar auto-tests
// de audio y luces escribiendo comandos. Ideal para verificar una placa recién
// armada o depurar una conexión sospechosa antes de cargar un firmware musical.
//
// Cada ~250 ms imprime un bloque con:
//   - IMU      : WHO_AM_I + aceleración (g), giro (deg/s) y temperatura (C)
//   - BOTONES  : estado de los 5 botones (--- / PUL)
//   - POTS     : lectura cruda 0..4095 + porcentaje de cada potenciómetro
//   - PIEZOS   : lectura cruda de los 4 piezos (golpéalos para ver el pico)
//   - EXT      : lectura cruda de los 2 sensores externos
// ==============================================================================================================================================
// FUNCIONAMIENTO — comandos por el Monitor Serie (escribe la letra y pulsa Enter)
// ==============================================================================================================================================
//   a  → Test de AUDIO: 3 beeps por el DAC (debes oírlos en el parlante/auriculares)
//   l  → Test de LEDS : barrido de color por la tira (requiere ENABLE_LED_TEST = 1)
//   p  → Pausa / reanuda el volcado continuo (para leer con calma)
//   h  → Ayuda (vuelve a mostrar esta lista)
// ==============================================================================================================================================

// ---- Activa/desactiva auto-tests pesados (dependencias de librerías/periféricos) ----
#define ENABLE_AUDIO_TEST 1   // 1 = compila el test de tono I2S (solo core, sin libs externas)
#define ENABLE_LED_TEST   0   // 1 = compila el test de tira WS2812 (REQUIERE instalar FastLED)

#include <Wire.h>

// ---------------------------------- PINES ----------------------------------
#define SDA_PIN   21
#define SCL_PIN   38
#define IMU_ADDR  0x68

#define NUM_BTN 5
const uint8_t BTN_PINS[NUM_BTN]   = {44, 42, 0, 45, 47};

#define NUM_POT 4
const uint8_t POT_PINS[NUM_POT]   = {1, 2, 8, 10};

#define NUM_PIEZO 4
const uint8_t PIEZO_PINS[NUM_PIEZO] = {4, 5, 6, 7};

#define NUM_EXT 2
const uint8_t EXT_PINS[NUM_EXT]   = {3, 9};

// ---------------------------------- AUDIO (I2S / PCM5102) ----------------------------------
#if ENABLE_AUDIO_TEST
  #include <driver/i2s_std.h>
  #define I2S_LCK   39
  #define I2S_DIN   40
  #define I2S_BCK   41
  #define SAMPLE_RATE 44100
  static i2s_chan_handle_t tx_chan;

  void i2s_init() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));
    i2s_std_config_t std_cfg = {
      .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = (gpio_num_t)I2S_BCK,
        .ws   = (gpio_num_t)I2S_LCK,
        .dout = (gpio_num_t)I2S_DIN,
        .din  = I2S_GPIO_UNUSED,
        .invert_flags = { false, false, false },
      },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
  }

  // Reproduce un tono senoidal de 'freq' Hz durante 'ms' milisegundos
  void playTone(float freq, int ms) {
    const int N = 256;
    int16_t buf[N * 2];               // estéreo intercalado
    float phase = 0.0f;
    float inc = 2.0f * 3.14159265f * freq / SAMPLE_RATE;
    int totalSamples = (int)((long)SAMPLE_RATE * ms / 1000);
    int done = 0;
    size_t written;
    while (done < totalSamples) {
      for (int i = 0; i < N; i++) {
        int16_t s = (int16_t)(sinf(phase) * 9000);   // volumen moderado
        buf[i * 2] = s; buf[i * 2 + 1] = s;
        phase += inc; if (phase > 6.2831853f) phase -= 6.2831853f;
      }
      i2s_channel_write(tx_chan, buf, sizeof(buf), &written, portMAX_DELAY);
      done += N;
    }
  }

  void audioTest() {
    Serial.println(">> Test de AUDIO: deberias oir 3 beeps (440, 660, 880 Hz)...");
    playTone(440, 200); delay(80);
    playTone(660, 200); delay(80);
    playTone(880, 250);
    Serial.println(">> Test de audio finalizado. Si NO oiste nada: revisa DAC, I2S (39/40/41) y Flash Mode = DIO.");
  }
#endif

// ---------------------------------- LEDS (WS2812) ----------------------------------
#if ENABLE_LED_TEST
  #include <FastLED.h>
  #define LED_PIN    46
  #define NUM_LEDS   68          // ajusta a tu tira (los primeros 6 son SMD internos)
  CRGB leds[NUM_LEDS];

  void led_init() { FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS); FastLED.setBrightness(60); }

  void ledTest() {
    Serial.println(">> Test de LEDS: barrido R, G, B por la tira...");
    CRGB cols[3] = { CRGB::Red, CRGB::Green, CRGB::Blue };
    for (int c = 0; c < 3; c++) {
      for (int i = 0; i < NUM_LEDS; i++) { leds[i] = cols[c]; FastLED.show(); delay(8); }
      fill_solid(leds, NUM_LEDS, CRGB::Black); FastLED.show(); delay(150);
    }
    Serial.println(">> Test de LEDS finalizado.");
  }
#endif

// ---------------------------------- IMU ----------------------------------
uint8_t imuWho = 0xFF;
bool imuOk = false;

// Lee el registro WHO_AM_I (0x75). Devuelve el valor o 0xFF si no responde.
// IMPORTANTE: secuencia IDÉNTICA a firmwares/test_imu/test_imu.ino (que SÍ funciona).
// No revisar el retorno de endTransmission(false): en el core de ESP32 ese
// repeated-start puede devolver != 0 aunque el IMU esté presente.
uint8_t imuReadWho() {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x75);
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, 1, true);
  return Wire.available() ? Wire.read() : 0xFF;
}

// Lee accel(3)+temp+gyro(3) crudos. Devuelve true si llegaron los 14 bytes.
// ESTA es la prueba real de que el IMU está vivo — NO el valor de WHO_AM_I
// (muchos MPU6050 clones devuelven 0x70/0x72/0x98, no 0x68). test_imu.ino
// muestra datos en cada loop sin gatear en WHO_AM_I: hacemos lo mismo.
bool imuReadRaw(int16_t &ax, int16_t &ay, int16_t &az, int16_t &tp,
                int16_t &gx, int16_t &gy, int16_t &gz) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, 14, true);
  if (Wire.available() < 14) return false;
  ax = (Wire.read() << 8) | Wire.read();
  ay = (Wire.read() << 8) | Wire.read();
  az = (Wire.read() << 8) | Wire.read();
  tp = (Wire.read() << 8) | Wire.read();
  gx = (Wire.read() << 8) | Wire.read();
  gy = (Wire.read() << 8) | Wire.read();
  gz = (Wire.read() << 8) | Wire.read();
  return true;
}

void imu_init() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  // Despertar el MPU (PWR_MGMT_1 = 0) y darle tiempo.
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);
  Wire.endTransmission(true);
  delay(100);

  imuWho = imuReadWho();   // solo informativo (acepta cualquier valor)
}

// ---------------------------------- LECTURAS AUX ----------------------------------
// Promedio de 8 lecturas para estabilizar el ADC
int readADC(uint8_t pin) {
  long sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return (int)(sum / 8);
}

// ---------------------------------- ESTADO ----------------------------------
bool paused = false;
unsigned long lastDump = 0;

void printHelp() {
  Serial.println();
  Serial.println("================ PERCU-SYNTH · TEST DE SISTEMA ================");
  Serial.println(" Comandos (escribe la letra + Enter):");
  Serial.println("   a  -> Test de AUDIO (3 beeps por el DAC)");
#if ENABLE_LED_TEST
  Serial.println("   l  -> Test de LEDS (barrido por la tira WS2812)");
#endif
  Serial.println("   p  -> Pausa / reanuda el volcado continuo");
  Serial.println("   h  -> Esta ayuda");
  Serial.println("===============================================================");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(600);

  for (int i = 0; i < NUM_BTN; i++) pinMode(BTN_PINS[i], INPUT_PULLUP);
  analogReadResolution(12);

  imu_init();
#if ENABLE_AUDIO_TEST
  i2s_init();
#endif
#if ENABLE_LED_TEST
  led_init();
#endif

  printHelp();

  // Detección real: probar si el acelerómetro devuelve datos (no depende de WHO_AM_I)
  { int16_t a,b,c,d,e,f,g; imuOk = imuReadRaw(a,b,c,d,e,f,g); }
  Serial.print("IMU WHO_AM_I = 0x"); Serial.print(imuWho, HEX);
  Serial.println(imuOk ? "  >> IMU OK :) (responde con datos)"
                       : "  >> IMU sin datos al arranque (se reintenta en cada lectura)");
  Serial.println("Mueve el equipo, gira los pots, pulsa botones y golpea los piezos para verlos cambiar.\n");
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r' || c == ' ') continue;
    switch (c) {
#if ENABLE_AUDIO_TEST
      case 'a': case 'A': audioTest(); break;
#endif
#if ENABLE_LED_TEST
      case 'l': case 'L': ledTest(); break;
#endif
      case 'p': case 'P':
        paused = !paused;
        Serial.println(paused ? ">> PAUSA (escribe 'p' para reanudar)" : ">> Reanudado");
        break;
      case 'h': case 'H': printHelp(); break;
      default:
        Serial.print(">> Comando desconocido: '"); Serial.print(c); Serial.println("'  (escribe 'h' para ayuda)");
    }
  }
}

void dumpStatus() {
  // ---- IMU ----
  // La detección se basa en si llegan datos (14 bytes), NO en WHO_AM_I.
  Serial.print("IMU  ");
  int16_t ax, ay, az, tp, gx, gy, gz;
  if (imuReadRaw(ax, ay, az, tp, gx, gy, gz)) {
    imuOk = true;
    Serial.print("acc[g] x="); Serial.print(ax / 16384.0f, 2);
    Serial.print(" y=");       Serial.print(ay / 16384.0f, 2);
    Serial.print(" z=");       Serial.print(az / 16384.0f, 2);
    Serial.print("  gyro[d/s] x="); Serial.print(gx / 131.0f, 0);
    Serial.print(" y=");             Serial.print(gy / 131.0f, 0);
    Serial.print(" z=");             Serial.print(gz / 131.0f, 0);
    Serial.print("  T="); Serial.print(tp / 340.0f + 36.53f, 1);
    Serial.print("C  (WHO=0x"); Serial.print(imuWho, HEX); Serial.println(")");
  } else {
    imuOk = false;
    Serial.println("NO detectado (sin datos en I2C — revisa SDA 21 / SCL 38 / 3.3V / GND)");
  }

  // ---- BOTONES ----
  Serial.print("BTN  ");
  for (int i = 0; i < NUM_BTN; i++) {
    bool pressed = (digitalRead(BTN_PINS[i]) == LOW);   // pull-up: LOW = pulsado
    Serial.print(i + 1); Serial.print(":");
    Serial.print(pressed ? "PUL" : "---");
    Serial.print("(g"); Serial.print(BTN_PINS[i]); Serial.print(")  ");
  }
  Serial.println();

  // ---- POTS ----
  Serial.print("POT  ");
  for (int i = 0; i < NUM_POT; i++) {
    int v = readADC(POT_PINS[i]);
    Serial.print(i + 1); Serial.print(":");
    Serial.print(v); Serial.print("("); Serial.print(map(v, 0, 4095, 0, 100)); Serial.print("%)  ");
  }
  Serial.println();

  // ---- PIEZOS ----
  Serial.print("PZ   ");
  for (int i = 0; i < NUM_PIEZO; i++) {
    Serial.print(i + 1); Serial.print(":");
    Serial.print(readADC(PIEZO_PINS[i])); Serial.print("  ");
  }
  Serial.println();

  // ---- SENSORES EXTERNOS ----
  Serial.print("EXT  A:"); Serial.print(readADC(EXT_PINS[0]));
  Serial.print("  B:");    Serial.print(readADC(EXT_PINS[1]));
  Serial.println();

  Serial.println("---------------------------------------------------------------");
}

void loop() {
  handleSerial();

  if (!paused && millis() - lastDump >= 250) {
    lastDump = millis();
    dumpStatus();
  }
}
