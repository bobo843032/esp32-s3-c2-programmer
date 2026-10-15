#pragma once
// Endast värdsimulering: inga Arduino-/USB-/GPIO-anrop till hårdvara.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <deque>
#include <utility>
#define INPUT 1
#define OUTPUT 3
#define INPUT_PULLUP 5
#define INPUT_PULLDOWN 9
#define HIGH 1
#define LOW 0
struct Edge { int dMode, dValue; };
inline int modes[64] = {}, levels[64] = {};
inline std::deque<int> ckReads, dReads;
inline std::vector<Edge> edges;
inline std::vector<std::pair<int,int>> configurations;
inline uint32_t ticks = 0;
inline void pinMode(uint8_t p, uint8_t m) { modes[p]=m; configurations.emplace_back(p,m); }
inline void digitalWrite(uint8_t p, uint8_t v) {
  if (p==6 && levels[p]==LOW && v==HIGH && modes[p]==OUTPUT) edges.push_back({modes[5],levels[5]});
  levels[p]=v;
}
inline int digitalRead(uint8_t p) {
  auto &q = p==6 ? ckReads : dReads;
  if (q.empty()) return p==6 ? HIGH : LOW;
  int v=q.front(); q.pop_front(); return v;
}
inline void delay(uint32_t ms) { ticks+=ms; }
inline void delayMicroseconds(uint32_t us) { ticks+=(us+999)/1000; }
inline uint32_t millis() { return ticks; }
inline void noInterrupts() {}
inline void interrupts() {}
class String {
  std::string value;
public:
  String(const char *s=""):value(s){}
  void trim(){} void toLowerCase(){}
  size_t length()const{return value.size();}
  bool operator==(const char *s)const{return value==s;}
};
struct FakeUSB {
  std::string output;
  void setRxBufferSize(int) {} void begin(int) {}
  int available(){return 0;} char read(){return 0;}
  String readStringUntil(char){return String();}
  void print(const char*s){output+=s;}
  void println(const char*s=""){output+=std::string(s)+"\n";}
  void printf(const char *f,...) { char b[1024];va_list a;va_start(a,f);vsnprintf(b,sizeof b,f,a);va_end(a);output+=b; }
};
inline FakeUSB Serial;
inline void resetMock() {
  std::memset(modes,0,sizeof modes);std::memset(levels,0,sizeof levels);
  ckReads.clear();dReads.clear();edges.clear();configurations.clear();ticks=0;Serial.output.clear();
  modes[6]=OUTPUT;levels[6]=HIGH;
}
inline void queueByte(uint8_t b) { for(int i=0;i<8;i++)dReads.push_back((b>>i)&1); }
