// String de Arduino sobre std::string (la misma interfaz que usa arduino-esp32).
#pragma once
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifndef DEC
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2
#endif

class __FlashStringHelper;
#define F(s) (s)
#define FPSTR(s) (s)

class String {
  std::string s;
  static std::string num(unsigned long long v, int base) {
    if (base < 2 || base > 36) base = 10;
    char buf[70]; int i = 68; buf[69] = 0;
    if (v == 0) buf[i--] = '0';
    while (v) { int d = v % base; buf[i--] = d < 10 ? '0' + d : 'a' + d - 10; v /= base; }
    return std::string(buf + i + 1);
  }
  static std::string snum(long long v, int base) {
    if (base == 10 && v < 0) return "-" + num((unsigned long long)(-v), 10);
    return num((unsigned long long)v, base);
  }
  static std::string flt(double v, int dec) { char b[64]; snprintf(b, sizeof b, "%.*f", dec, v); return b; }
public:
  String() {}
  String(const char *c) : s(c ? c : "") {}
  String(const std::string &c) : s(c) {}
  String(const String &o) = default;
  String(String &&o) = default;
  String(char c) : s(1, c) {}
  String(unsigned char v, unsigned char base = 10) : s(num(v, base)) {}
  String(int v, unsigned char base = 10) : s(snum(v, base)) {}
  String(unsigned int v, unsigned char base = 10) : s(num(v, base)) {}
  String(long v, unsigned char base = 10) : s(snum(v, base)) {}
  String(unsigned long v, unsigned char base = 10) : s(num(v, base)) {}
  String(long long v, unsigned char base = 10) : s(snum(v, base)) {}
  String(unsigned long long v, unsigned char base = 10) : s(num(v, base)) {}
  String(float v, unsigned char dec = 2) : s(flt(v, dec)) {}
  String(double v, unsigned char dec = 2) : s(flt(v, dec)) {}
  String &operator=(const String &o) = default;
  String &operator=(String &&o) = default;
  String &operator=(const char *c) { s = c ? c : ""; return *this; }

  unsigned int length() const { return (unsigned int)s.size(); }
  bool isEmpty() const { return s.empty(); }
  const char *c_str() const { return s.c_str(); }
  char *begin() { return &s[0]; }
  char *end() { return &s[0] + s.size(); }
  bool reserve(unsigned int n) { s.reserve(n); return true; }
  const std::string &std() const { return s; }

  bool concat(const String &o) { s += o.s; return true; }
  bool concat(const char *c) { if (c) s += c; return true; }
  bool concat(char c) { s += c; return true; }
  template <typename T> bool concat(T v) { s += String(v).s; return true; }
  String &operator+=(const String &o) { s += o.s; return *this; }
  String &operator+=(const char *c) { if (c) s += c; return *this; }
  String &operator+=(char c) { s += c; return *this; }
  template <typename T> String &operator+=(T v) { s += String(v).s; return *this; }

  friend String operator+(const String &a, const String &b) { return String(a.s + b.s); }
  friend String operator+(const String &a, const char *b) { return String(a.s + (b ? b : "")); }
  friend String operator+(const char *a, const String &b) { return String((a ? a : "") + b.s); }
  friend String operator+(const String &a, char b) { return String(a.s + b); }
  template <typename T> friend String operator+(const String &a, T b) { return String(a.s + String(b).s); }

  bool operator==(const String &o) const { return s == o.s; }
  bool operator==(const char *c) const { return s == (c ? c : ""); }
  bool operator!=(const String &o) const { return s != o.s; }
  bool operator!=(const char *c) const { return s != (c ? c : ""); }
  bool operator<(const String &o) const { return s < o.s; }
  bool operator>(const String &o) const { return s > o.s; }
  bool equals(const String &o) const { return s == o.s; }
  bool equalsIgnoreCase(const String &o) const {
    if (s.size() != o.s.size()) return false;
    for (size_t i = 0; i < s.size(); i++) if (tolower((unsigned char)s[i]) != tolower((unsigned char)o.s[i])) return false;
    return true;
  }
  int compareTo(const String &o) const { return s.compare(o.s); }
  bool startsWith(const String &p) const { return s.compare(0, p.s.size(), p.s) == 0; }
  bool endsWith(const String &p) const { return s.size() >= p.s.size() && s.compare(s.size() - p.s.size(), p.s.size(), p.s) == 0; }

  char charAt(unsigned int i) const { return i < s.size() ? s[i] : 0; }
  void setCharAt(unsigned int i, char c) { if (i < s.size()) s[i] = c; }
  char operator[](unsigned int i) const { return charAt(i); }
  char &operator[](unsigned int i) { return s[i]; }
  void getBytes(unsigned char *buf, unsigned int n, unsigned int idx = 0) const {
    if (!n) return; size_t k = 0; for (; k + 1 < n && idx + k < s.size(); k++) buf[k] = s[idx + k]; buf[k] = 0; }
  void toCharArray(char *buf, unsigned int n, unsigned int idx = 0) const { getBytes((unsigned char *)buf, n, idx); }

  int indexOf(char c, unsigned int from = 0) const { size_t p = s.find(c, from); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(const String &t, unsigned int from = 0) const { size_t p = s.find(t.s, from); return p == std::string::npos ? -1 : (int)p; }
  int lastIndexOf(char c) const { size_t p = s.rfind(c); return p == std::string::npos ? -1 : (int)p; }
  int lastIndexOf(char c, unsigned int from) const { size_t p = s.rfind(c, from); return p == std::string::npos ? -1 : (int)p; }
  int lastIndexOf(const String &t) const { size_t p = s.rfind(t.s); return p == std::string::npos ? -1 : (int)p; }
  String substring(unsigned int a) const { return a >= s.size() ? String() : String(s.substr(a)); }
  String substring(unsigned int a, unsigned int b) const {
    if (a > b) { unsigned t = a; a = b; b = t; }
    if (a >= s.size()) return String();
    return String(s.substr(a, (b > s.size() ? s.size() : b) - a)); }

  void replace(char a, char b) { for (auto &c : s) if (c == a) c = b; }
  void replace(const String &a, const String &b) {
    if (a.s.empty()) return; size_t p = 0;
    while ((p = s.find(a.s, p)) != std::string::npos) { s.replace(p, a.s.size(), b.s); p += b.s.size(); } }
  void remove(unsigned int i) { if (i < s.size()) s.erase(i); }
  void remove(unsigned int i, unsigned int n) { if (i < s.size()) s.erase(i, n); }
  void toLowerCase() { for (auto &c : s) c = tolower((unsigned char)c); }
  void toUpperCase() { for (auto &c : s) c = toupper((unsigned char)c); }
  void trim() {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    s = s.substr(a, b - a); }

  long toInt() const { return atol(s.c_str()); }
  float toFloat() const { return (float)atof(s.c_str()); }
  double toDouble() const { return atof(s.c_str()); }
  explicit operator bool() const { return true; }
};
