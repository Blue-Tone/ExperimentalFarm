#include <avr/sleep.h>
#include <time.h>
#include <TimeLib.h>
#include <DS3232RTC.h>    //http://github.com/JChristensen/DS3232RTC

// スリープと復帰　本家
// http://playground.arduino.cc/Learning/ArduinoSleepCode

#include <Wire.h>
#define RTC_EEPROM_ADDR 0x57
#define  WAKE_PIN 2         // pin used for waking up
#define  WAKE_PIN2 0         // pin used for waking up
#define PUMP_PIN 2          // ポンプピン
#define PUMP_OPEN_TIME 500  // [ms]
#define WATER_INTERVAL_TIME 1000*60 // 1000*60*60*24// 水やり間隔

void setup() {
  Serial.begin(9600);
  Serial.println("start setup()");

  pinMode(PUMP_PIN, OUTPUT);
  pinMode(WAKE_PIN, INPUT);

  setSyncProvider(RTC.get); // the function to get the time from the RTC
  if (timeStatus() != timeSet){
    Serial.println("Unable to sync with the RTC");
  }else{
    Serial.println("RTC has set the system time");
  }

  Serial.println("end   setup()");
}

void loop() {
  Serial.println("start loop()");

  // スリープ復帰用アラーム設定
  byte seconds, minutes, hours = 0;
  //RTC.setAlarm(ALM2_MATCH_MINUTES, seconds, minutes, hours, dowSunday);  
  RTC.setAlarm(ALM1_EVERY_SECOND, seconds, minutes, hours, dowSunday);  
  
  // ★ToDo:前回保存時間読み出し
  // ★ToDo:水やり期間経過していたら、水やり実行

  digitalWrite(PUMP_PIN, HIGH);  // ポンプON
  delay(PUMP_OPEN_TIME);
  digitalWrite(PUMP_PIN, LOW);  // ポンプOFF

  // ★ToDo:前回動作時間をEEPROMに保存
  unsigned long ul_now = now();
  Serial.println(ul_now);
  digitalClockDisplay();
  
  // ★ToDo:スリープ
  delay(1000);
  sleepNow();
  Serial.println("end   loop()");
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

/*
//WRITE!!!!*******************************
 Wire.beginTransmission(address);
 Wire.write(0x00);      //First Word Address
 Wire.write(0x00);      //Second Word Address

 Wire.write(0x41);      //Write an 'A'

 delay(10);

 Wire.endTransmission();
 delay(10);


 //READ!!!!*********************************
 Wire.beginTransmission(address);
 Wire.write(0x00);      //First Word Address
 Wire.write(0x00);      //Second Word Address
 Wire.endTransmission();
 delay(10);

 Wire.requestFrom(address, 1);
 delay(10);
 data = Wire.read();
 Serial.write(data);      //Read the data and print to Serial port
 Serial.println();
 delay(10);

*/


void wakeUpNow()        // here the interrupt is handled after wakeup
{
    Serial.println("start wakeUpNow()");
  // execute code here after wake-up before returning to the loop() function
  // timers and code using timers (serial.print and more...) will not work here.
  // we don't really need to execute any special functions here, since we
  // just want the thing to wake up
    Serial.println("end   wakeUpNow()");
}

void sleepNow()         // here we put the arduino to sleep
{
    /* Now is the time to set the sleep mode. In the Atmega8 datasheet
     * http://www.atmel.com/dyn/resources/prod_documents/doc2486.pdf on page 35
     * there is a list of sleep modes which explains which clocks and
     * wake up sources are available in which sleep mode.
     *
     * In the avr/sleep.h file, the call names of these sleep modes are to be found:
     *
     * The 5 different modes are:
     *     SLEEP_MODE_IDLE         -the least power savings
     *     SLEEP_MODE_ADC
     *     SLEEP_MODE_PWR_SAVE
     *     SLEEP_MODE_STANDBY
     *     SLEEP_MODE_PWR_DOWN     -the most power savings
     *
     * For now, we want as much power savings as possible, so we
     * choose the according
     * sleep mode: SLEEP_MODE_PWR_DOWN
     *
     */  
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);   // sleep mode is set here
 
    sleep_enable();          // enables the sleep bit in the mcucr register
                             // so sleep is possible. just a safety pin
 
    /* Now it is time to enable an interrupt. We do it here so an
     * accidentally pushed interrupt button doesn't interrupt
     * our running program. if you want to be able to run
     * interrupt code besides the sleep function, place it in
     * setup() for example.
     *
     * In the function call attachInterrupt(A, B, C)
     * A   can be either 0 or 1 for interrupts on pin 2 or 3.  
     *
     * B   Name of a function you want to execute at interrupt for A.
     *
     * C   Trigger mode of the interrupt pin. can be:
     *             LOW        a low level triggers
     *             CHANGE     a change in level triggers
     *             RISING     a rising edge of a level triggers
     *             FALLING    a falling edge of a level triggers
     *
     * In all but the IDLE sleep modes only LOW can be used.
     */
 
    Serial.println("call sleep_mode()");
    delay(100);

    attachInterrupt(WAKE_PIN2,wakeUpNow, CHANGE); // use interrupt 0 (pin 2) and run function
                                       // wakeUpNow when pin 2 gets LOW
 
    sleep_mode();            // here the device is actually put to sleep!!
                             // THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP
    Serial.println("called sleep_mode()");
 
    sleep_disable();         // first thing after waking from sleep:
                             // disable sleep...
    detachInterrupt(WAKE_PIN2);      // disables interrupt 0 on pin 2 so the
                             // wakeUpNow code will not be executed
                             // during normal running time.
 
}
 
 
