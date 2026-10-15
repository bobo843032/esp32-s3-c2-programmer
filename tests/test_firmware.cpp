#include "Arduino.h"
#include <cassert>
#ifdef TEST_READER
#include "../firmware/ESP32-C2-Reader/ESP32-C2-Reader.ino"
#else
#include "../firmware/ESP32-C2-Programmer/c2_core.cpp"
#include "../firmware/ESP32-C2-Programmer/ESP32-C2-Programmer.ino"
#endif
static void expectFrame(size_t count, int ins0, int ins1) {
  assert(edges.size()==count);
  assert(edges[0].dMode==INPUT);
  assert(edges[1].dMode==OUTPUT && edges[1].dValue==ins0);
  assert(edges[2].dMode==OUTPUT && edges[2].dValue==ins1);
  assert(edges.back().dMode==INPUT);
}
static void commandInfo() {
#ifdef TEST_READER
  cmdId();
#else
  linkUp=flashUp=armed=false;cmdInfo();
#endif
}
int main() {
  resetMock();
#ifdef TEST_READER
  c2WriteAR(0x96);
#else
  writeAR(0x96);
#endif
  expectFrame(12,1,1);
  for(int i=0;i<8;i++)assert(edges[3+i].dValue==((0x96>>i)&1));
  resetMock();dReads={0,1};queueByte(0x09);uint8_t b=0;
#ifdef TEST_READER
  assert(c2ReadDR(b));
#else
  assert(readDR(b)==C2_OK);
#endif
  assert(b==0x09);expectFrame(16,0,0);
  for(size_t i=5;i<edges.size();i++)assert(edges[i].dMode==INPUT);
#ifndef TEST_READER
  resetMock();queueByte(0xA5);assert(readAR()==0xA5);expectFrame(12,0,1);
  resetMock();dReads={0,1};assert(writeDR(0x96)==C2_OK);expectFrame(16,1,0);
  for(int i=0;i<8;i++)assert(edges[5+i].dValue==((0x96>>i)&1));
  resetMock();uint32_t n=0;assert(parseNum("0xFFFFFFFF",n)&&n==UINT32_MAX);
  assert(!parseNum("-1",n));assert(!parseNum("4294967296",n));assert(!parseNum("123z",n));
  resetMock();tgt=findTarget(0x09);linkUp=true;flashUp=true;
  char a0[]="READ",a1[]="0xFFFFFFFF",a2[]="1";char *args[]={a0,a1,a2};
  cmdRead(args,3);assert(Serial.output.find("ERR RANGE")==0 && edges.empty());
  resetMock();linkUp=true;flashUp=true;armed=true;char w0[]="WRITE",w2[]="00";char *wa[]={w0,a1,w2};
  cmdWrite(wa,3);assert(Serial.output.find("ERR RANGE")==0 && edges.empty());
#endif
  resetMock();ckReads={0,0};commandInfo();assert(Serial.output.find("ERR CKLOW")==0);assert(edges.empty());
  for(auto p:configurations)assert(p.second==INPUT);
  resetMock();ckReads={1,0};commandInfo();assert(Serial.output.find("ERR CKUNSTABLE")==0 && edges.empty());
  resetMock();ckReads={1,1,0};commandInfo();assert(Serial.output.find("ERR RESET")==0);
  assert(edges.size()==1);assert(modes[6]==INPUT && modes[5]==INPUT);
  resetMock();ckReads={1,1,1};commandInfo();assert(Serial.output.find("ERR COMM")==0);
  assert(modes[6]==INPUT && modes[5]==INPUT);
  resetMock();ckReads={1,1,1};dReads.push_back(1);queueByte(0x09);dReads.push_back(1);queueByte(0x01);
  commandInfo();assert(Serial.output.find("ERR")==std::string::npos);
#ifdef TEST_READER
  assert(Serial.output.find("Device ID  : 0x09")!=std::string::npos);
#else
  assert(Serial.output.find("OK dev=09 rev=01") == 0);
  // INFO får inte välja FPCTL/FPDAT eller göra en Data Write.
  assert(edges.size()==55); // reset + (AR 12 + DR 15) två gånger
#endif
  for(auto p:configurations)assert(p.second!=INPUT_PULLDOWN && p.second!=INPUT_PULLUP);
  puts("PASS: firmware frames, safety gate, reset-level failure, COMM timeout, ID-only path");
}
