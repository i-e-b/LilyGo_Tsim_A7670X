#define TINY_GSM_MODEM_SIM7600   //The AT instruction of A7670 is compatible with SIM7600
#define TINY_GSM_RX_BUFFER 1024  // Set RX buffer to 1Kb
#define SerialAT Serial1

// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP 600          // Time ESP32 will go to sleep (in seconds)

#define UART_BAUD 115200
#define PIN_DTR 25
#define PIN_TX 26
#define PIN_RX 27
#define BAT_ADC 35
#define MODEM_ENABLE 12
#define MODEM_POWER 4
#define PIN_RI 33
#define RESET 5

#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCLK 14
#define SD_CS 13

// Real-time clock (used to get reset type)
#include <rom/rtc.h>

// GSM client
#include <TinyGsmClient.h>
#include "Arduino.h"

#ifdef DUMP_AT_COMMANDS  // if enabled it requires the streamDebugger lib
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

void print_reset_reason(RESET_REASON reason)
{
  switch ( reason)
  {
    case 1 : Serial.println ("POWERON_RESET");break;          /**<1, Vbat power on reset*/
    case 3 : Serial.println ("SW_RESET");break;               /**<3, Software reset digital core*/
    case 4 : Serial.println ("OWDT_RESET");break;             /**<4, Legacy watch dog reset digital core*/
    case 5 : Serial.println ("DEEPSLEEP_RESET");break;        /**<5, Deep Sleep reset digital core*/
    case 6 : Serial.println ("SDIO_RESET");break;             /**<6, Reset by SLC module, reset digital core*/
    case 7 : Serial.println ("TG0WDT_SYS_RESET");break;       /**<7, Timer Group0 Watch dog reset digital core*/
    case 8 : Serial.println ("TG1WDT_SYS_RESET");break;       /**<8, Timer Group1 Watch dog reset digital core*/
    case 9 : Serial.println ("RTCWDT_SYS_RESET");break;       /**<9, RTC Watch dog Reset digital core*/
    case 10 : Serial.println ("INTRUSION_RESET");break;       /**<10, Instrusion tested to reset CPU*/
    case 11 : Serial.println ("TGWDT_CPU_RESET");break;       /**<11, Time Group reset CPU*/
    case 12 : Serial.println ("SW_CPU_RESET");break;          /**<12, Software reset CPU*/
    case 13 : Serial.println ("RTCWDT_CPU_RESET");break;      /**<13, RTC Watch dog Reset CPU*/
    case 14 : Serial.println ("EXT_CPU_RESET");break;         /**<14, for APP CPU, reseted by PRO CPU*/
    case 15 : Serial.println ("RTCWDT_BROWN_OUT_RESET");break;/**<15, Reset when the vdd voltage is not stable*/
    case 16 : Serial.println ("RTCWDT_RTC_RESET");break;      /**<16, RTC Watch dog reset digital core and rtc module*/
    default : Serial.println ("NO_MEAN");
  }
}

// Send an 'AT' command to the SIMCOM module.
// This will re-try on timeout
int sendCommand(const char* cmd) {
  int i = 10;
  while (i) {
    SerialAT.println(cmd);
    if (i == 10) delay(1500);  // extra delay 1st time

    for (int j=0; j<5; j++){ // wait for reply
      delay(500);
      if (SerialAT.available()) {
        String r = SerialAT.readString();
        Serial.println(r);
        if (r.indexOf("OK") >= 0) {
          return true;
        }
      }
    }
    i--; // No 'OK', so try again
  }
  return false;
}

// Enable, power-up and reset the modem
void modemTurnOn() {
  Serial.println(F("Resetting Modem...\r\n"));

  // Set the A7670 enable line (?)
  pinMode(MODEM_ENABLE, OUTPUT);
  digitalWrite(MODEM_ENABLE, HIGH);

  // A7670 Reset (if already up?)
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, LOW);
  delay(100);
  digitalWrite(RESET, HIGH);
  delay(3000);
  digitalWrite(RESET, LOW);

  // Cycle modem power (?)
  pinMode(MODEM_POWER, OUTPUT);
  digitalWrite(MODEM_POWER, LOW);
  delay(100);
  digitalWrite(MODEM_POWER, HIGH);
  delay(1000);
  digitalWrite(MODEM_POWER, LOW);

  Serial.println(F("Modem On\r\n"));
}

// Turn the modem off.
void modemTurnOff(){
  Serial.println(F("Deactivating Modem...\r\n"));

  int ok = Serial.print("AT+CPOF"); // send a power-down command. The SIMCOM manages its own power.

  if (ok) {
    Serial.println(F("\r\nThe SIMCOM A7670 Module replied OK to POWER-DOWN command. Lights should go out."));
  } else {
    Serial.println(F("******************************************************************"));
    Serial.println(F("** Failed to connect to the modem! Check the baud and try again.**"));
    Serial.println(F("******************************************************************\r\n"));
  }
}

// Open a PC -> SIMCOM terminal session, mediated by the ESP32.
// Typing something wrong here *could* damage the modem, so 
// it's deactivated by default
void startInteractiveSession(){
    while (true) {
        if (SerialAT.available()) {
            Serial.write(SerialAT.read());
        }
        if (Serial.available()) {
            SerialAT.write(Serial.read());
        }
        delay(1);
    }
}

void setup() {
  Serial.begin(115200);  // Set console baud rate         PC <-> ESP32
  delay(100);
  SerialAT.begin(115200, SERIAL_8N1, PIN_RX, PIN_TX);  // ESP32 <-> SIMCOM
  delay(2000);

  // Output reset types
  RESET_REASON core0 = rtc_get_reset_reason(0);
  RESET_REASON core1 = rtc_get_reset_reason(1);
  Serial.println("CPU0 reset reason: ");
  print_reset_reason(core0);

  Serial.println("CPU1 reset reason: ");
  print_reset_reason(core1);

  if (core0 == 5 || core1 == 5){
    Serial.println("Woke from deep-sleep. Not starting modem");
    return;
  }

  // turn the modem on
  modemTurnOn();
  delay(1000);

  // test with an 'AT' command
  Serial.println(F("Testing Modem Response...\r\n"));
  int reply = sendCommand("AT");
  Serial.println(F("Test finished\r\n"));


  if (reply) {
    Serial.println("\r\n");
    Serial.println(F("\r\nThe SIMCOM A7670 Module replied OK to attention command. Looks good."));
  } else {
    Serial.println(F("******************************************************************"));
    Serial.println(F("** Failed to connect to the modem! Check the baud and try again.**"));
    Serial.println(F("******************************************************************\r\n"));
  }

  // Try to power the modem down
  modemTurnOff();
}

void loop() {
  // Test is complete Set ESP32 to sleep mode
  Serial.print("Z");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.print("z");
  delay(200);
  esp_deep_sleep_start(); // never returns. We will get reset with DEEPSLEEP_RESET
}