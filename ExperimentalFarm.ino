#include <time.h>
#include <TimeLib.h>
#include <DS3232RTC.h>    //http://github.com/JChristensen/DS3232RTC
 
#define PUMP_PIN 2          // ポンプピン
#define PUMP_OPEN_TIME 500  // [ms]
#define WATER_INTERVAL_TIME 1000*60 // 1000*60*60*24// 水やり間隔

void setup() {
  Serial.begin(9600);
  pinMode(PUMP_PIN, OUTPUT);

  setSyncProvider(RTC.get); // the function to get the time from the RTC
  if (timeStatus() != timeSet){
    Serial.println("Unable to sync with the RTC");
  }else{
    Serial.println("RTC has set the system time");
  }
}

void loop() {
  // ★ToDo:前回保存時間読み出し
  // ★ToDo:水やり期間経過していたら、水やり実行

  digitalWrite(PUMP_PIN, HIGH);  // ポンプON
  delay(PUMP_OPEN_TIME);
  digitalWrite(PUMP_PIN, LOW);  // ポンプOFF

  // ★ToDo:前回動作時間をEEPROMに保存
  
  // ★ToDo:スリープ
  delay(PUMP_OPEN_TIME);

  digitalClockDisplay();
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


