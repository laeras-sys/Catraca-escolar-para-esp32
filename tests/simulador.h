#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <sys/time.h>

using byte = unsigned char;
const int LOW=0, HIGH=1, INPUT_PULLUP=2, OUTPUT=3, HEX=16;
const int HSPI=1, WIFI_STA=1, WL_CONNECTED=3, CARD_NONE=0;
const char* const FILE_READ="r";
const char* const FILE_APPEND="a";

class String {
 public:
  std::string s;
  String()=default;
  String(const char* x):s(x?x:""){}
  String(const std::string& x):s(x){}
  String(char x):s(1,x){}
  String(int x):s(std::to_string(x)){}
  String(unsigned x):s(std::to_string(x)){}
  String(unsigned long x):s(std::to_string(x)){}
  String(byte x,int base) {std::ostringstream o; if(base==HEX)o<<std::hex; o<<int(x); s=o.str();}
  const char* c_str()const{return s.c_str();}
  size_t length()const{return s.size();}
  String substring(size_t i,size_t j=std::string::npos)const{return i<s.size()?s.substr(i,j==std::string::npos?j:j-i):"";}
  void toUpperCase(){for(char& c:s)c=std::toupper(static_cast<unsigned char>(c));}
  bool startsWith(const char* p)const{return s.find(p)==0;}
  void remove(size_t i,size_t n){s.erase(i,n);}
  void trim(){while(!s.empty()&&std::isspace(static_cast<unsigned char>(s.back())))s.pop_back(); while(!s.empty()&&std::isspace(static_cast<unsigned char>(s.front())))s.erase(s.begin());}
  String& operator+=(char c){s+=c;return *this;}
  String& operator+=(const String& x){s+=x.s;return *this;}
  friend String operator+(String a,const String& b){a+=b;return a;}
  friend bool operator==(const String& a,const String& b){return a.s==b.s;}
  friend bool operator!=(const String& a,const String& b){return !(a==b);}
  friend std::ostream& operator<<(std::ostream& o,const String& a){return o<<a.s;}
};

unsigned long tempoMs=0, atrasoRedeMs=0;
time_t tempoEpoch=0;
std::map<int,int> entradas;
std::vector<std::string> eventos;
unsigned long millis(){return tempoMs;}
void pinMode(int,int){}
int digitalRead(int p){return entradas.count(p)?entradas[p]:HIGH;}
void digitalWrite(int p,int v){entradas[p]=v;}
time_t tempoControlado(time_t* destino){if(destino)*destino=tempoEpoch;return tempoEpoch;}
#define time tempoControlado

struct SerialFake {
  std::string texto;
  void begin(int){}
  template<class T> size_t print(const T& x){std::ostringstream o; o<<x; texto+=o.str();return o.str().size();}
  template<class T> size_t println(const T& x){return print(x)+print("\n");}
  size_t println(){return print("\n");}
} Serial;
struct WireFake{void begin(int,int){}} Wire;
class SPIClass{public: explicit SPIClass(int=0){} void begin(int=-1,int=-1,int=-1,int=-1){}};
SPIClass SPI;
class LiquidCrystal_I2C {
 public:
  std::string linhas[2];int linha=0;
  LiquidCrystal_I2C(int,int,int){}
  void init(){} void backlight(){} void clear(){linhas[0]=linhas[1]="";}
  void setCursor(int,int l){linha=l;}
  template<class T>void print(const T& x){std::ostringstream o;o<<x;linhas[linha]+=o.str();}
};
class Servo {
 public:
  int angulo=-1,pino=-1;
  void setPeriodHertz(int){}
  void attach(int p,int,int){pino=p;eventos.push_back("servo_attach");}
  void write(int a){angulo=a;eventos.push_back(a==0?"servo_fechado":"servo_aberto");}
};
class MFRC522 {
 public:
  struct UID {byte size=0;byte uidByte[10]={};} uid;
  std::deque<std::string> cartoes;
  MFRC522(int,int){}
  void PCD_Init(){} void PCD_DumpVersionToSerial(){} void PICC_HaltA(){} void PCD_StopCrypto1(){}
  bool PICC_IsNewCardPresent(){return !cartoes.empty();}
  bool PICC_ReadCardSerial(){if(cartoes.empty())return false; const std::string c=cartoes.front();cartoes.pop_front();uid.size=c.size()/2;for(byte i=0;i<uid.size;i++)uid.uidByte[i]=std::stoul(c.substr(i*2,2),nullptr,16);return true;}
};
struct WiFiFake {
  bool conectado=false;
  int chamadasBegin=0;
  int status(){tempoMs+=atrasoRedeMs;return conectado?WL_CONNECTED:0;}
  String localIP(){return "192.0.2.1";}
  void reconnect(){} void mode(int){} void setAutoReconnect(bool){} void begin(const char*,const char*){++chamadasBegin;}
} WiFi;
void configTime(long,int,const char*,const char*){}
void sntp_set_time_sync_notification_cb(void(*)(timeval*)){}

struct DiscoFake {
  std::map<std::string,std::string> arquivos;
  bool falharAbertura=false,perderFlush=false,corromperFlush=false;
  size_t limiteEscrita=std::string::npos;
} disco;
class File {
  struct Handle{std::string nome,buffer;size_t pos=0;bool escrita=false,aberto=true;};
  std::shared_ptr<Handle> h;
 public:
  File()=default;
  File(const std::string& n,bool escrita):h(std::make_shared<Handle>()) {h->nome=n;h->escrita=escrita;h->pos=escrita?disco.arquivos[n].size():0;}
  explicit operator bool()const{return h&&h->aberto;}
  size_t size()const{return *this?disco.arquivos[h->nome].size()+h->buffer.size():0;}
  size_t position()const{return *this?h->pos:0;}
  bool seek(size_t pos){if(!*this||pos>size())return false;h->pos=pos;return true;}
  size_t available()const{return *this&&h->pos<size()?size()-h->pos:0;}
  int read(){if(!available())return -1;return static_cast<unsigned char>(disco.arquivos[h->nome][h->pos++]);}
  size_t read(uint8_t* p,size_t n){size_t r=0;for(;r<n;++r){int v=read();if(v<0)break;p[r]=v;}return r;}
  size_t write(const uint8_t* p,size_t n){if(!*this||!h->escrita)return 0;n=std::min(n,disco.limiteEscrita);h->buffer.append(reinterpret_cast<const char*>(p),n);h->pos+=n;return n;}
  size_t print(const char* p){return write(reinterpret_cast<const uint8_t*>(p),strlen(p));}
  size_t print(const String& p){return print(p.c_str());}
  String readStringUntil(char c){String s;while(available()){int x=read();if(x==c)break;s+=static_cast<char>(x);}return s;}
  void flush(){if(!*this||!h->escrita)return;if(!disco.perderFlush){if(disco.corromperFlush&&!h->buffer.empty())h->buffer.back()='X';disco.arquivos[h->nome]+=h->buffer;}h->buffer.clear();}
  void close(){if(*this){flush();h->aberto=false;}}
};
struct SDFake {
  bool begin(int,SPIClass&,uint32_t){eventos.push_back("sd_begin");return true;}
  int cardType(){return 1;}
  bool exists(const char* p){return disco.arquivos.count(p);}
  File open(const char* p,const char* modo){if(disco.falharAbertura)return {};bool escrita=std::string(modo)=="a";if(!escrita&&!exists(p))return {};return File(p,escrita);}
} SD;
