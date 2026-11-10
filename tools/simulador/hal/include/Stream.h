// Print y Stream de Arduino (la parte de texto de Serial, Wire, etc.)
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "WString.h"
#include <type_traits>

class Print {
  size_t printNumber(unsigned long long n, uint8_t base) {
    char buf[70]; char *p = &buf[69]; *p = 0;
    if (base < 2) base = 10;
    do { int d = n % base; *--p = d < 10 ? '0' + d : 'A' + d - 10; n /= base; } while (n);
    return write(p);
  }
  size_t printSigned(long long n, int base) {
    if (base == 0) return write((uint8_t)n);
    if (base == 10 && n < 0) { size_t k = print('-'); return k + printNumber((unsigned long long)(-n), 10); }
    return printNumber((unsigned long long)n, base);
  }
  size_t printFloat(double v, int dig) {
    if (isnan(v)) return print("nan");
    if (isinf(v)) return print("inf");
    char b[64]; snprintf(b, sizeof b, "%.*f", dig, v); return write(b);
  }
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t *buf, size_t n) { size_t k = 0; while (n--) k += write(*buf++); return k; }
  size_t write(const char *s) { return s ? write((const uint8_t *)s, strlen(s)) : 0; }
  size_t write(const char *buf, size_t n) { return write((const uint8_t *)buf, n); }
  // write(0x6B), write(n): enteros de cualquier tipo van como un byte (como en arduino-esp32)
  template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, uint8_t>::value>::type>
  size_t write(T v) { return write((uint8_t)v); }
  virtual int availableForWrite() { return 0; }
  virtual void flush() {}

  size_t print(const char *s) { return write(s); }
  size_t print(const String &s) { return write(s.c_str()); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(unsigned char n, int base = 10) { return printNumber(n, base); }
  size_t print(int n, int base = 10) { return printSigned(n, base); }
  size_t print(unsigned int n, int base = 10) { return printNumber(n, base); }
  size_t print(long n, int base = 10) { return printSigned(n, base); }
  size_t print(unsigned long n, int base = 10) { return printNumber(n, base); }
  size_t print(long long n, int base = 10) { return printSigned(n, base); }
  size_t print(unsigned long long n, int base = 10) { return printNumber(n, base); }
  size_t print(double n, int dig = 2) { return printFloat(n, dig); }
  size_t print(bool b) { return printNumber(b ? 1 : 0, 10); }

  size_t println() { return write("\r\n"); }
  template <typename T> size_t println(const T &v) { size_t k = print(v); return k + println(); }
  template <typename T> size_t println(const T &v, int fmt) { size_t k = print(v, fmt); return k + println(); }

  size_t printf(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
    char loc[256]; va_list a; va_start(a, fmt);
    int n = vsnprintf(loc, sizeof loc, fmt, a); va_end(a);
    if (n < 0) return 0;
    if ((size_t)n < sizeof loc) return write((const uint8_t *)loc, n);
    char *big = new char[n + 1]; va_start(a, fmt); vsnprintf(big, n + 1, fmt, a); va_end(a);
    size_t k = write((const uint8_t *)big, n); delete[] big; return k;
  }
};

class Stream : public Print {
protected:
  unsigned long _timeout = 1000;
  int timedRead();
  int timedPeek();
public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  void setTimeout(unsigned long t) { _timeout = t; }
  unsigned long getTimeout() const { return _timeout; }
  size_t readBytes(char *buf, size_t n);
  size_t readBytes(uint8_t *buf, size_t n) { return readBytes((char *)buf, n); }
  size_t readBytesUntil(char term, char *buf, size_t n);
  String readString();
  String readStringUntil(char term);
  long parseInt();
  float parseFloat();
  bool find(const char *target);
};
