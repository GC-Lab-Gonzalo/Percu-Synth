// I2C de la placa virtual: un registro de dispositivos por dirección, más los
// dos módulos del header de la placa, que aparecen o desaparecen según el SimBus:
// el MPU6050 (0x68) y la pantalla OLED SSD1306 (0x3C). Comparten el header, así
// que la interfaz pone uno o el otro.
#include "Wire.h"
#include <emscripten.h>

TwoWire Wire(0);
TwoWire Wire1(1);
static SimI2CDevice *g_disp[128];

void sim_i2c_attach(SimI2CDevice *d) { if (d) g_disp[d->address() & 127] = d; }
void sim_i2c_detach(uint8_t addr) { g_disp[addr & 127] = nullptr; }

// ---------------------------------------------------------------- MPU6050
// Registros como el chip real: WHO_AM_I = 0x68, arranca dormido (PWR_MGMT_1 = 0x40)
// hasta que el sketch escribe 0 en 0x6B; escala según ACCEL_CONFIG / GYRO_CONFIG;
// los datos (0x3B..0x48) se leen en ráfaga con autoincremento.
// Un golpe al piso (botón de la interfaz) suma una sacudida amortiguada a la
// aceleración: un pico de unos g que suena y se apaga en ~50 ms, sobre todo en Z.
class SimMPU6050 : public SimI2CDevice {
  uint8_t reg[128];
  uint8_t ptr = 0;
  uint32_t piso_visto = 0;
  double piso_t0 = -1e9;
  float piso_a = 0;
  static void be16(uint8_t *p, int v) { if (v > 32767) v = 32767; if (v < -32768) v = -32768; p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
  static float ruido(float a) { return ((int)(esp_random() % 2001) - 1000) / 1000.0f * a; }
public:
  SimMPU6050() { reset(); }
  void reset() { memset(reg, 0, sizeof reg); reg[0x75] = 0x68; reg[0x6B] = 0x40; ptr = 0; }
  uint8_t address() const override { return 0x68; }
  void write(const uint8_t *d, size_t n) override {
    if (!n) return;
    ptr = d[0] & 127;
    for (size_t i = 1; i < n; i++) {
      uint8_t r = ptr & 127;
      if (r == 0x6B && (d[i] & 0x80)) { reset(); continue; }      // DEVICE_RESET
      if (r != 0x75) reg[r] = d[i];
      ptr = (ptr + 1) & 127;
    }
  }
  size_t read(uint8_t *out, size_t n) override {
    SimBus *b = sim_bus();
    double ahora = emscripten_get_now();
    if (b->piso_seq != piso_visto) { piso_visto = b->piso_seq; piso_t0 = ahora; piso_a = b->piso_amp; }
    double t = ahora - piso_t0;                    // ms desde el golpe
    float golpe = 0;
    if (t >= 0 && t < 80) golpe = piso_a * (float)(exp(-t / 9.0) * cos(2 * M_PI * 0.055 * t));   // ~55 Hz amortiguado
    uint8_t datos[14];
    bool dormido = reg[0x6B] & 0x40;
    if (dormido) memset(datos, 0, sizeof datos);
    else {
      static const float LSB_G[4] = { 16384, 8192, 4096, 2048 };
      static const float LSB_DPS[4] = { 131, 65.5f, 32.8f, 16.4f };
      float ka = LSB_G[(reg[0x1C] >> 3) & 3], kg = LSB_DPS[(reg[0x1B] >> 3) & 3];
      const float reparto[3] = { 0.18f, -0.12f, 1.0f };   // el piso empuja sobre todo en Z
      for (int i = 0; i < 3; i++) be16(datos + i * 2, (int)lroundf((b->imu_acc[i] + golpe * reparto[i] + ruido(0.004f)) * ka));
      be16(datos + 6, (int)lroundf((26.0f - 36.53f) * 340.0f));   // 26 °C
      for (int i = 0; i < 3; i++) be16(datos + 8 + i * 2, (int)lroundf((b->imu_gyr[i] + golpe * 40.0f * reparto[2 - i] + ruido(0.15f)) * kg));
    }
    for (size_t i = 0; i < n; i++) {
      uint8_t r = (ptr + i) & 127;
      out[i] = (r >= 0x3B && r <= 0x48) ? datos[r - 0x3B] : reg[r];
    }
    ptr = (ptr + n) & 127;
    return n;
  }
};
static SimMPU6050 g_mpu;

// ---------------------------------------------------------------- SSD1306
// La pantalla OLED 128×64 como el chip: el primer byte de cada transacción es el
// byte de control (Co, D/C). Con D/C = 0 lo que sigue son comandos (con sus
// argumentos, que pueden llegar en transacciones separadas); con D/C = 1 son
// datos que se escriben en la RAM (8 páginas de 128 columnas, un bit por pixel,
// el bit 0 arriba) avanzando según el modo de direccionamiento (0x20).
// La RAM y la configuración se publican en el SimBus; la interfaz arma la imagen
// aplicando el espejo de columnas (A0/A1), el de filas (C0/C8), la línea de inicio,
// el desplazamiento, el contraste y la inversión, igual que el panel real.
class SimSSD1306 : public SimI2CDevice {
  uint8_t modo = 2;                 // 0 horizontal, 1 vertical, 2 página (el del reset)
  uint8_t col = 0, pag = 0;
  uint8_t col0 = 0, col1 = 127, pag0 = 0, pag1 = 7;
  uint8_t cmd = 0, args[8], nargs = 0, falta = 0;
  static int argumentos(uint8_t c) {
    switch (c) {
      case 0x20: case 0x81: case 0x8D: case 0xA8: case 0xD3: case 0xD5: case 0xD9: case 0xDA: case 0xDB: return 1;
      case 0x21: case 0x22: case 0xA3: return 2;
      case 0x26: case 0x27: return 6;
      case 0x29: case 0x2A: return 5;
      default: return 0;
    }
  }
  void publicar() { __atomic_add_fetch(&sim_bus()->oled_seq, 1, __ATOMIC_RELEASE); }
  void ejecutar() {
    volatile int32_t *cfg = sim_bus()->oled_cfg;
    switch (cmd) {
      case 0xAE: cfg[0] = 0; break;
      case 0xAF: cfg[0] = 1; break;
      case 0xA6: cfg[1] = 0; break;
      case 0xA7: cfg[1] = 1; break;
      case 0x81: cfg[2] = args[0]; break;
      case 0xA0: cfg[3] = 0; break;
      case 0xA1: cfg[3] = 1; break;
      case 0xC0: cfg[4] = 0; break;
      case 0xC8: cfg[4] = 1; break;
      case 0xD3: cfg[6] = args[0] & 63; break;
      case 0xA4: cfg[7] = 0; break;
      case 0xA5: cfg[7] = 1; break;
      case 0x20: modo = args[0] & 3; if (modo == 3) modo = 2; break;
      case 0x21: col0 = args[0] & 127; col1 = args[1] & 127; col = col0; break;
      case 0x22: pag0 = args[0] & 7; pag1 = args[1] & 7; pag = pag0; break;
      default:
        if (cmd >= 0x40 && cmd <= 0x7F) cfg[5] = cmd & 63;                 // línea de inicio
        else if (cmd >= 0xB0 && cmd <= 0xB7) pag = cmd & 7;                // página (modo página)
        else if (cmd <= 0x0F) col = (col & 0xF0) | cmd;                    // columna, nibble bajo
        else if (cmd >= 0x10 && cmd <= 0x1F) col = (uint8_t)(((cmd & 7) << 4) | (col & 0x0F));
        break;                                                             // el resto (reloj, bomba, scroll…) no cambia la imagen
    }
    publicar();
  }
  void comando(uint8_t c) {
    if (falta) { args[nargs++] = c; if (--falta == 0) ejecutar(); return; }
    cmd = c; nargs = 0; falta = (uint8_t)argumentos(c);
    if (!falta) ejecutar();
  }
  void dato(uint8_t d) {
    sim_bus()->oled_ram[(pag & 7) * 128 + (col & 127)] = d;
    if (modo == 2) { col = (col + 1) & 127; return; }             // modo página: no cambia de página
    if (modo == 0) {
      if (col >= col1) { col = col0; pag = pag >= pag1 ? pag0 : pag + 1; } else col++;
    } else {
      if (pag >= pag1) { pag = pag0; col = col >= col1 ? col0 : col + 1; } else pag++;
    }
  }
public:
  void reset() {
    modo = 2; col = pag = 0; col0 = 0; col1 = 127; pag0 = 0; pag1 = 7; falta = 0; nargs = 0;
    SimBus *b = sim_bus();
    memset(b->oled_ram, 0, sizeof b->oled_ram);
    const int32_t def[8] = { 0, 0, 0x7F, 0, 0, 0, 0, 0 };
    for (int i = 0; i < 8; i++) b->oled_cfg[i] = def[i];
    publicar();
  }
  uint8_t address() const override { return 0x3C; }
  void write(const uint8_t *d, size_t n) override {
    size_t i = 0;
    bool hubo_datos = false;
    while (i < n) {
      uint8_t ctrl = d[i++];
      bool co = ctrl & 0x80, esDato = ctrl & 0x40;
      if (co) {                                     // un solo byte y después otro byte de control
        if (i < n) { if (esDato) { dato(d[i]); hubo_datos = true; } else comando(d[i]); i++; }
        continue;
      }
      for (; i < n; i++) { if (esDato) { dato(d[i]); hubo_datos = true; } else comando(d[i]); }
    }
    if (hubo_datos) publicar();
  }
  size_t read(uint8_t *out, size_t n) override {
    // byte de estado: bit 6 = pantalla apagada
    for (size_t i = 0; i < n; i++) out[i] = sim_bus()->oled_cfg[0] ? 0x00 : 0x40;
    return n;
  }
};
static SimSSD1306 g_oled;
static int g_oled_puesta = -1;

static SimI2CDevice *buscar(uint8_t addr) {
  addr &= 127;
  SimBus *b = sim_bus();
  if (addr == 0x68) {
    if (!b->imu_on) { g_mpu.reset(); return nullptr; }   // sacado del header: vuelve dormido
    return &g_mpu;
  }
  if (addr == 0x3C) {
    int on = b->oled_on ? 1 : 0;
    if (on != g_oled_puesta) { g_oled_puesta = on; g_oled.reset(); }   // enchufarla = encenderla de cero
    return on ? &g_oled : nullptr;
  }
  return g_disp[addr];
}

uint8_t TwoWire::endTransmission(bool) {
  transmitting = false;
  SimI2CDevice *d = buscar(txAddr);
  if (!d) return 2;                       // nadie respondió a la dirección: NACK
  d->write(txBuf, txLen);
  return 0;
}
size_t TwoWire::pedir(uint8_t addr, size_t len, bool) {
  rxLen = rxPos = 0;
  SimI2CDevice *d = buscar(addr);
  if (!d) return 0;
  if (len > sizeof rxBuf) len = sizeof rxBuf;
  rxLen = d->read(rxBuf, len);
  return rxLen;
}
