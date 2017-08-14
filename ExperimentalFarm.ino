// ■参考サイト
// スリープと復帰　本家
// http://playground.arduino.cc/Learning/ArduinoSleepCode

// ArduinoでDS3231 RTCモジュールを使った定期処理
// http://programresource.net/2015/04/23/2547.html

// ■接続
// DS3231 RTC
//   SDA A4
//   SCL A5
//   SQW D2
#define WAKE_PIN        2       // pin used for waking up

// ポンプリレー
#define PUMP_PIN        3       // ポンプピン

// 土壌水分センサ AnalogOut
#define MOISTURE_PIN    A0      // 土壌水分センサのピン

#define TONE_PIN        4       // デバッグ用トーンピン
#define DEBUG_PIN       11      // デバッグモードピン 起動時にGNDと接続でデバッグモードで起動。割り込み毎分・水やり間隔２分
#define DATA_CLEAR_PIN  12      // データ消去ピン 起動時にGNDと接続でデータの書き込み位置を初期化

// 調整パラメータ
#define MOISTURE_THRESHOLD  500     // 土壌水分センサの水やり判定の閾値。閾値以上なら水やり。
#define TEMP_ADJUST         -3      // 温度誤差補正

#include <avr/sleep.h>
#include <avr/power.h>

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#include <time.h>
#include <TimeLib.h>
#include <DS3232RTC.h>    //http://github.com/JChristensen/DS3232RTC

// eepromのライブラリ
#include <AT24CX.h>       // https://github.com/cyberp/AT24Cx
AT24C32 mem(7); // addr=57;   // EEPROM object 4kbyte 4096

// 測定データ構造体 16バイト
struct MEASURE_DATA {
  time_t sTime;   // 日時 4byte
  short temp;     // 温度 2byte
  short moisture; // 土壌水分センサー 200湿<->1024乾 2byte
  byte isExec;    // 水やり実行フラグ 1byte
  byte a;
  byte b;
  byte c;
  long d; // 4byte
  // 残水量
};

#define MEM_POS_ADDR        0x00    // 測定データ書き込み位置
#define MEM_LAST_TIME_ADDR  0x04    // 前回実行時間書き込み位置
#define MEM_DATA_OFFSET     0x10    // データ開始アドレスオフセット
// 0x00 addr_poss 1byte
// 0x04 lastExecTime 4byte
// 0x10 MEASURE_DATA 16bytes
// 0x20 ・・・
// 0x30 ・・・
 
#define RTC_EEPROM_ADDR 0x57
#define PUMP_OPEN_TIME 10000// [ms]

unsigned long WATER_INTERVAL_TIME = 60*60*24;   // 水やり間隔[s] 60*60*24=毎時
unsigned long WATER_INTERVAL_TIME_DEBUG = 60*2; // デバッグ用 60*2=2分

byte WAKE_INTERVAL = ALM2_MATCH_MINUTES;        // 割込み間隔 毎時
byte WAKE_INTERVAL_DEBUG = ALM2_EVERY_MINUTE;   // デバッグ用 毎分

#define ALM_INDEX 2 // 1 or 2

#define TONE_FREQ 440
#define SERIAL_SPEED 9600
//#define SERIAL_SPEED 115200

bool rtcint = false;

// セットアップ
void setup() {
  Serial.begin(SERIAL_SPEED);
  Serial.println("start setup()");

  // Pin初期化
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(WAKE_PIN, INPUT);
  pinMode(DATA_CLEAR_PIN, INPUT_PULLUP);
  pinMode(DEBUG_PIN, INPUT_PULLUP);

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

  // デバッグモード
  if(LOW == digitalRead(DEBUG_PIN)){
    Serial.println("Debug mode");
    WAKE_INTERVAL = WAKE_INTERVAL_DEBUG;
    WATER_INTERVAL_TIME = WATER_INTERVAL_TIME_DEBUG;
  }
  
  // データ初期化
  if(LOW == digitalRead(DATA_CLEAR_PIN)){
    Serial.println("Clear Data!!!");
    mem.write(MEM_POS_ADDR, 0);// データ書き込み位置を初期化
    mem.writeLong(MEM_LAST_TIME_ADDR, now() - WATER_INTERVAL_TIME); // 前回実行日時を現在-実行間隔に設定
  }

  printMemRowData();  // eepromの生データ表示
  printMemData();     // eepromの整形データ表示
  
  //show current time, sync with 0 second
  digitalClockDisplay();
  synctozero();
 
  //set alarm to fire every minute
  RTC.alarm(ALM_INDEX);
  attachInterrupt(digitalPinToInterrupt(WAKE_PIN), alcall, FALLING);
  RTC.setAlarm(WAKE_INTERVAL , 0, 0, 0);
  RTC.alarmInterrupt(ALM_INDEX, true);
  digitalClockDisplay();
  Serial.println("end   setup()");
}

// ループ
void loop() {
  //Serial.println("start loop()");
  
  // 割込みで起動後のみ実行
  if (rtcint) {
    rtcint = false;

    setSyncProvider(RTC.get); // the function to get the time from the RTC
    if (timeStatus() != timeSet){
      Serial.println("Unable to sync with the RTC");
    }
    RTC.alarm(ALM_INDEX);
  }
  digitalClockDisplay();
  
  // 前回保存時間読み出し
  time_t lastTime = mem.readLong(MEM_LAST_TIME_ADDR);
  Serial.print("lastTime : ");  
  printTime(lastTime);
  Serial.println("");  
  
  // posインクリして保存
  int pos = mem.read(MEM_POS_ADDR);
  if(4000 < pos) pos = 0;//　4000で0に戻す
  mem.write(MEM_POS_ADDR, ++pos);
  Serial.println(pos, HEX);
  
  MEASURE_DATA data = {0};
  data.sTime = now();
  data.temp = (short)( (RTC.temperature() / 4 ) * 10.0);
  data.moisture = analogRead(MOISTURE_PIN);
  Serial.println(data.moisture);

  // 水やり期間経過、土壌水分センサの値が閾値以上なら、水やり実行
  if(now() >= lastTime + WATER_INTERVAL_TIME){
    Serial.println("past the interval.");
    if(MOISTURE_THRESHOLD < data.moisture){
      Serial.println("over threshold. exec water server!");
      digitalWrite(PUMP_PIN, HIGH); // ポンプON
      delay(PUMP_OPEN_TIME);
      digitalWrite(PUMP_PIN, LOW);  // ポンプOFF
      data.isExec = true;
      mem.writeLong(MEM_LAST_TIME_ADDR, data.sTime);
    }else{
      Serial.println("not over threshold");
    }
  }else{
    Serial.println("not past the interval. sleep again");
  }

  // 測定データ書き込み
  mem.write(pos*sizeof(data), (byte*)&data, sizeof(data));

  //go to power save mode
  enterSleep();
  
  //Serial.println("end   loop()");
}

// 00秒まで待機
void synctozero() {
  //Serial.println("start synctozero()");
  //wait until second reaches 0
  while (second() != 0) {
    delay(100);
  }
  //Serial.println("end   synctozero()");
}

// アラームコール 割込み処理
void alcall() {
  //Serial.println("start alcall()");
  //per minute interrupt call
  rtcint = true;
  //Serial.println("end   alcall()");
}
 
// スリープ実行
void enterSleep(void)
{
  Serial.println("start enterSleep()>>>");
  cbi(ADCSRA,ADEN);                     // ADC 回路電源をOFF (ADC使って無くても120μA消費するため）
  // シリアル出力のため、少し待つ
  tone(TONE_PIN, TONE_FREQ);
  delay(100);
  noTone(TONE_PIN);
  Serial.flush();
  delay(100);
     
  //enter sleep mode to save power
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();
 
  sleep_disable();
  power_all_enable();
  sbi(ADCSRA,ADEN);                     // ADC ON
  Serial.begin(SERIAL_SPEED);
  Serial.println("end   enterSleep()<<<");
  tone(TONE_PIN, TONE_FREQ);
  delay(100);
  noTone(TONE_PIN);
}

// 現在時刻、温度出力
void digitalClockDisplay() {
  Serial.print("now      : ");
  printTime(now());
  Serial.print(", ");
  
  float c = RTC.temperature() / 4;
  Serial.print(c + TEMP_ADJUST);
//  Serial.print((char)223);
  Serial.print("度C, 水分");
  Serial.println(analogRead(MOISTURE_PIN));
}

// 日時表示
void printTime(time_t t){
  Serial.print(year(t));
  Serial.print("/");
  printDigits(month(t));
  Serial.print("/");
  printDigits(day(t));
  Serial.print(" ");

  printDigits(hour(t));
  Serial.print(":");
  printDigits(minute(t));
  Serial.print(":");
  printDigits(second(t));
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


// eepromの生データ表示
void printMemRowData(){
  int pos = mem.read(MEM_POS_ADDR);
  Serial.println(pos, HEX);
  
  for(int i = 0; i <= pos; i++){
    Serial.print("0x");
    printHexDigits(i);
    Serial.print("  ");
    for(int j = 0; j < 16; j++){
      int val = mem.read(i*16+j);
      printHexDigits(val);
      Serial.print("  ");
    }
    Serial.println("");
  }  
}

// eepromの整形データ表示
void printMemData(){
  int pos = mem.read(MEM_POS_ADDR);
  Serial.println(pos, HEX);
  
  for(int i = 1; i <= pos; i++){
    MEASURE_DATA data = {0};
    mem.read(i*sizeof(data), (byte*)&data, sizeof(data));

    printTime(data.sTime);
    Serial.print(", ");
    Serial.print(data.temp/10.0 + TEMP_ADJUST);
    Serial.print("度C, 水分");
    Serial.print(data.moisture);
    Serial.print(", 実行フラグ");
    Serial.println(data.isExec);
  }
}


