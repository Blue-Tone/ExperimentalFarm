// スリープと復帰　本家
// http://playground.arduino.cc/Learning/ArduinoSleepCode

// ArduinoでDS3231 RTCモジュールを使った定期処理
// http://programresource.net/2015/04/23/2547.html

#include <avr/sleep.h>
#include <avr/power.h>
#include <time.h>
#include <TimeLib.h>
#include <DS3232RTC.h>    //http://github.com/JChristensen/DS3232RTC

// eepromのライブラリ
#include <AT24CX.h>       // https://github.com/cyberp/AT24Cx
#include <Wire.h>
#define MEM_I2C_ADDR 8
AT24C32 mem(7); // addr=57;   // EEPROM object 4kbyte 4096

// 0x00 addr_poss

// 測定データ構造体 16バイト
struct MEASURE_DATA {
  long sTime;  // 日時 4byte
  short temp; // 温度 2byte
  short a; // 2byte
  long b; // 4byte
  long c; // 4byte
};

#define MEM_POS_ADDR    0x00  // データ書き込み位置
#define MEM_DATA_OFFSET 0x10  // データ開始アドレスオフセット
#define MEM_DATA_SIZE   0x10  // データサイズ
// 0x10 time long 4 bytes
// 0x11 temperature float 4 bytes →x10計算後の2バイトで保存
// 水分
// 残水量
// 

#define RTC_EEPROM_ADDR 0x57
#define WAKE_PIN 2          // pin used for waking up
#define WAKE_PIN_INDEX 0    // for attachInterrupt()
#define PUMP_PIN 3          // ポンプピン
#define PUMP_OPEN_TIME 500  // [ms]
#define WATER_INTERVAL_TIME 1000*60 // 1000*60*60*24// 水やり間隔

#define ALM_INDEX 2 // 1 or 2

#define TONE_PIN 11
#define TONE_FREQ 4000

bool rtcint = false;

void setup() {
  Serial.begin(9600);
  Serial.println("start setup()");

  // Pin初期化
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(WAKE_PIN, INPUT);

  // RTC初期化
  RTC.alarmInterrupt(1, false);
  RTC.alarmInterrupt(2, false);
  RTC.oscStopped(true);

  // RTCから日時設定
  setSyncProvider(RTC.get); // the function to get the time from the RTC
  if (timeStatus() != timeSet){
    Serial.println("Unable to sync with the RTC");
  }else{
    Serial.println("RTC has set the system time");
  }

//  mem.write(0, 1);
  int pos = mem.read(MEM_POS_ADDR);
  Serial.println(pos, HEX);
  
  for(int i = 0; i <= pos; i++){
    Serial.print("0x");
    Serial.print(i, HEX);
      Serial.print("  ");
    for(int j = 0; j < 16; j++){
      int val = mem.read(i*16+j);
      printHexDigits(val);
      //Serial.print(val, HEX);
      Serial.print("  ");
    }
    Serial.println("");
  }  
  //show current time, sync with 0 second
  digitalClockDisplay();
  synctozero();
 
  //set alarm to fire every minute
  RTC.alarm(ALM_INDEX);
  attachInterrupt(WAKE_PIN_INDEX, alcall, FALLING);
  RTC.setAlarm(ALM2_EVERY_MINUTE , 0, 0, 0);
  RTC.alarmInterrupt(ALM_INDEX, true);
  digitalClockDisplay();
  Serial.println("end   setup()");
}

void loop() {
  Serial.println("start loop()");
  
  //process clock display and clear interrupt flag as needed
  if (rtcint) {
    rtcint = false;

    setSyncProvider(RTC.get); // the function to get the time from the RTC
    if (timeStatus() != timeSet){
      Serial.println("Unable to sync with the RTC");
    }
    
    digitalClockDisplay();
    RTC.alarm(2);

    // ★ToDo:前回保存時間読み出し
    // ★ToDo:水やり期間経過していたら、水やり実行
  
    digitalWrite(PUMP_PIN, HIGH);  // ポンプON
    delay(PUMP_OPEN_TIME);
    digitalWrite(PUMP_PIN, LOW);  // ポンプOFF
  
    // ★ToDo:前回動作時間をEEPROMに保存
    unsigned long ul_now = now();
//    AT24C32 mem = new AT24C32(7); // addr=57;   // EEPROM object 4kbyte 4096
    int pos = mem.read(MEM_POS_ADDR);
    Serial.println(pos, HEX);
    //mem.write(0, 1);
    mem.write(MEM_POS_ADDR, ++pos);
    pos = mem.read(MEM_POS_ADDR);
    Serial.println(pos, HEX);

    MEASURE_DATA data = {0};
    data.sTime = now();
    data.temp = (short)RTC.temperature() / 4 * 10;
    mem.write(pos*sizeof(data), (byte*)&data, sizeof(data));
    
  }
 
  //go to power save mode
  enterSleep();
  
  Serial.println("end   loop()");
  
}

// 00秒まで待機
void synctozero() {
  Serial.println("start synctozero()");
  //wait until second reaches 0
  while (second() != 0) {
    delay(100);
  }
  Serial.println("end   synctozero()");
}

// アラームコール 割込み処理
void alcall() {
  Serial.println("start alcall()");
  //per minute interrupt call
  rtcint = true;
  Serial.println("end   alcall()");
}
 
// スリープ実行
void enterSleep(void)
{
  Serial.println("start enterSleep()>>>");
  // シリアル出力のため、少し待つ
  tone(TONE_PIN, TONE_FREQ);
  delay(100);
  Serial.flush();
   
  //enter sleep mode to save power
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();
 
  sleep_disable();
  power_all_enable();
  Serial.begin(9600);
  Serial.println("end   enterSleep()<<<");
}

// 時刻出力
void digitalClockDisplay() {
  // digital clock display of the time
  Serial.print(year());
  Serial.print("/");
  printDigits(month());
  Serial.print("/");
  printDigits(day());
  Serial.print(" ");
  printDigits(hour());
  Serial.print(":");
  printDigits(minute());
  Serial.print(":");
  printDigits(second());
  Serial.print(" ");
  float c = RTC.temperature() / 4;
  Serial.print(c);
//  Serial.print((char)223);
  Serial.println("度C");
}

// ゼロ埋め出力
void printDigits(int digits) {
  if (digits < 10)Serial.print('0');
  Serial.print(digits);
}

// 16進ゼロ埋め出力
void printHexDigits(int digits) {
  if (digits < 16)Serial.print('0');
  Serial.print(digits, HEX);
}


//WRITE!!!!*******************************
void memWrite(byte addr1, byte addr2, byte val){
  Wire.beginTransmission(MEM_I2C_ADDR);
  Wire.write(addr1);      //First Word Address
  Wire.write(addr2);      //Second Word Address
  
  Wire.write(val);      //Write an val
  
  delay(10);
  
  Wire.endTransmission();
  delay(10);
}

//READ!!!!*********************************
byte memRead(byte addr1, byte addr2){
  Wire.beginTransmission(MEM_I2C_ADDR);
  Wire.write(addr1);      //First Word Address
  Wire.write(addr2);      //Second Word Address
  Wire.endTransmission();
  delay(10);
  
  Wire.requestFrom(MEM_I2C_ADDR, 1);
  delay(10);
  byte data = Wire.read();
  Serial.write(data);      //Read the data and print to Serial port
  Serial.println();
  delay(10);

  return data;
}


 
 
