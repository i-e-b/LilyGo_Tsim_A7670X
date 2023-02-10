// Real-time clock (used to get reset type)
#include <rom/rtc.h>
// Basic Arduino stuff (Long-term TODO: remove this and do the low level stuff ourself)
#include <Arduino.h>


#define SerialAT Serial1

#define uS_TO_S 1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP 60    // Time ESP32 will go to sleep (in seconds)

// USB Serial between PC and ESP32
#define USB_BAUD 9600

// UART comms between ESP32 and SIMCOM
#define UART_BAUD 115200
#define PIN_DTR 25
#define PIN_TX 26
#define PIN_RX 27
#define BAT_ADC 35
#define MODEM_ENABLE 12
#define MODEM_POWER 4
#define PIN_RI 33
#define RESET 5

// SD card pins
#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCLK 14
#define SD_CS 13

// Output to serial console, the reason a core reset.
// Upload program gives (1: "POWERON_RESET")
// Wake from sleep gives (5: "DEEPSLEEP_RESET")
void print_reset_reason(RESET_REASON reason){
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

// Tunable delay between AT commands
void atWait(){
  delay(500);
}

// Send an 'AT' command to the SIMCOM module.
// This will re-try on timeout
int sendCommand(const char* cmd) {
  int i = 10;
  while (i) {
    SerialAT.print(cmd);
    SerialAT.print("\r");

    if (i == 10) delay(500);  // extra delay 1st time

    for (int j=0; j<5; j++){ // wait for reply
      delay(500);
      if (SerialAT.available()) {
        String r = SerialAT.readString();
        Serial.print("> ");
        Serial.println(r);
        if (r.indexOf("OK") >= 0) {
          return true;
        }
        else if (r.indexOf("ERROR") >= 0){
          return false; // failed
        }
      }
    }
    i--; // No 'OK', so try again
  }
  delay(500);
  return false;
}

// Wait for a message to arrive on the AT interface
int waitForMessage(const char* terminate, int waitPeriod){
  int waited = 0;
  String needle = String(terminate);
  while (waited < waitPeriod){

    if (SerialAT.available()) {
        String r = SerialAT.readString();
        Serial.print("> ");
        Serial.println(r);
        Serial.println("<");
        if (r.indexOf(needle) >= 0) {
          Serial.printf("Found message '%s'; Continuing!\r\n", terminate);
          return true;
        }
      }

    Serial.print(".");
    delay(250);
    waited+=250;
  }
  Serial.printf("Did not see message '%s'\r\n", terminate);
  return false;
}

// Send an AT command to modem, with a variable numeric input
int sendCommandF(const char* format, int i){
  char* commandStr;
  if(0 > asprintf(&commandStr, format, i)) {
    Serial.println("Failed to generate command");
    return false;
  }
  Serial.println(&commandStr[0]);

  int result = sendCommand(&commandStr[0]);
  free(commandStr);
  return result;
}

// Send data to modem. Don't wait for any reply
void sendData(const char* data) {
  SerialAT.println(data);
  delay(500);
}

// Send data to modem, with a variable numeric input
void sendDataF(const char* format, int i){
  char* commandStr;
  if(0 > asprintf(&commandStr, format, i)) {
    Serial.println("Failed to generate command");
    return;
  }
  Serial.println(&commandStr[0]);

  sendData(&commandStr[0]);
  free(commandStr);
}

// Try to read data coming from the modem.
// Anything read will be output to serial
const char* tryReadModem() {
  for (int j = 0; j < 5; j++) {  // wait for reply
    delay(500);
    if (SerialAT.available()) {
      String r = SerialAT.readString();
      Serial.print("> ");
      Serial.print(r); // commands end in newlines
      return r.c_str();
    }
  }
  return NULL;
}

// Try to read data coming from the modem.
// Anything read will be output to serial
const char* tryReadModemQuiet() {
  for (int j = 0; j < 5; j++) {  // wait for reply
    delay(500);
    if (SerialAT.available()) {
      String r = SerialAT.readString();
      return r.c_str();
    }
  }
  return NULL;
}

// Send an 'AT' command to the SIMCOM module,
// and return any result as a string
const char* readCommand(const char* cmd){
  sendData(cmd);
  return tryReadModem();
}

// Send an 'AT' command to the SIMCOM module,
// and return any result as a string.
// This version does not echo results.
const char* readCommandQuiet(const char* cmd){
  sendData(cmd);
  return tryReadModemQuiet();
}

// Enable, power-up and reset the modem
// The modem is ready if this function returns 'true'
int modemTurnOn() {
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

  // Cycle modem power
  pinMode(MODEM_POWER, OUTPUT);
  digitalWrite(MODEM_POWER, LOW);
  delay(100);
  digitalWrite(MODEM_POWER, HIGH);
  delay(1000);
  digitalWrite(MODEM_POWER, LOW);

  Serial.println(F("Modem power-up starting\r\n"));
  delay(5000); // give it a while to boot


  int reply = waitForMessage("PB DONE", 12000);
  if (reply==false){
    Serial.println(F("** DID NOT SEE PB DONE message **"));
  }

  // test with an 'AT' command
  Serial.println(F("Testing Modem Response..."));
  reply = sendCommand("ATZ"); // Load user settings

  if (reply == false) {
    Serial.println(F("** Failed to connect to the modem! Check the baud and try again.**"));
    return false;
  }
  atWait();

  atWait();
  sendCommand("AT+CCLK?"); // get clock setting from modem
  atWait();
  Serial.println(F("Modem is active and ready"));
  return true;
}

// Send a power-off command to the SIMCOM modem
void modemTurnOff(){
  Serial.println("Powering off the SIMCOM unit");
  sendCommand("AT+CPOF"); // try to power-off the SIMCOM module.
  atWait();
}

// Ask the SIMCOM modem for the available telecom operators
// This is a very long list of the known operator IDs in the
// current firmware. It is NOT the available operators at the nearest tower.
void queryOperatorNames(){
  int reply = sendCommand("AT+COPN");
  if (reply == false) Serial.println("Failed to read operator list");
}

void setup() {
 // Connect to USB serial port if available
  Serial.begin(USB_BAUD);
  delay(100);

  // Output reset types
  RESET_REASON core0 = rtc_get_reset_reason(0);
  RESET_REASON core1 = rtc_get_reset_reason(1);
  print_reset_reason(core0);
  print_reset_reason(core1);

  // if we woke up from deep sleep, don't do anything.
  if (core0 == DEEPSLEEP_RESET || core1 == DEEPSLEEP_RESET){
    Serial.println("Woke from deep-sleep. Not starting modem");
    return; // jump to main loop, where we will re-enter deep sleep.
  }

  // Connect serial to the SIMCOM module
  SerialAT.begin(115200, SERIAL_8N1, PIN_RX, PIN_TX);  // ESP32 <-> SIMCOM
  delay(1000);

  // turn the modem on
  int reply = modemTurnOn();
  if (reply == false) {Serial.println(F("Failed to start SIMCOM modem")); return; }
  delay(1000);

  // We could now make HTTP calls
  Serial.println("Modem ready");

  // Request CPU temperature reading
  atWait();
  reply = sendCommand("AT+CPMUTEMP");
  if (reply==false) {Serial.println(F("Failed to read SIMCOM CPU temperature"));}

  // Request supply voltage
  atWait();
  reply = sendCommand("AT+CBC");
  if (reply==false) {Serial.println(F("Failed to read SIMCOM supply voltage"));}

  Serial.print("Set-up complete. Going to main loop ");
}

void loop() {
  // Drop directly into deep sleep
  modemTurnOff();
  atWait();
  Serial.print("Z");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S);
  Serial.print("z");
  delay(200);
  esp_deep_sleep_start(); // never returns. We will get reset with DEEPSLEEP_RESET
}