// Real-time clock (used to get reset type)
#include <rom/rtc.h>
// Basic Arduino stuff (Long-term TODO: remove this and do the low level stuff ourself)
#include <Arduino.h>


#define SerialAT Serial1
#define SerialEWC Serial2

#define S_TO_uS 1000000ULL  // Conversion factor for seconds to micro seconds
#define ONE_MINUTE_S 60    // Time ESP32 will go to sleep (in seconds)
#define ONE_HOUR_S 3600    // Time ESP32 will go to sleep (in seconds)

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

// UART comms between ESP32 and EWC
#define EWC_BAUD 9600
#define PIN_EWC_CTS 5
#define PIN_EWC_TX 21
#define PIN_EWC_RX 22

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

// Wait for a message to arrive on the AT interface,
// and return any extra data waiting after the message
const char* waitForMessageAndRead(const char* terminate, int waitPeriod, bool echo){
  int waited = 0;
  int index = -1;
  int len = strlen(terminate);
  if (len < 1) return NULL;

  String needle = String(terminate);

  while (waited < waitPeriod){

    if (SerialAT.available()) {
        String r = SerialAT.readString();
        if (echo){
          Serial.print("\n>>>\n");
          Serial.println(r);
          Serial.println("<<<");
        }

        index = r.indexOf(needle);
        if (index >= 0) {
          Serial.printf("Found message '%s'; Continuing!\r\n", terminate);
          return r.substring(index+len).c_str();
        }
      }

    Serial.print(".");
    delay(250);
    waited+=250;
  }
  Serial.printf("Did not see message '%s'\r\n", terminate);
  return NULL;
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

// Enable, power-up and reset the modem.
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

// Start socket services for sending raw UDP data
int modemEnableData(){
  int reply = sendCommand("AT+NETOPEN");
  if (reply == false) { Serial.println("Failed to open network session"); return false; }

  reply = sendCommand("AT+CIPOPEN=3,\"UDP\",,,42069"); // Open a UDP session on line 3, local port 42069
  if (reply == false) {
    Serial.println("Failed to open UDP session");
    sendCommand("AT+NETCLOSE"); // try to close data session
    return false;
  }

  return true;
}

// Stop socket services for sending raw UDP data
int modemDisableData(){
  int reply = sendCommand("AT+CIPCLOSE=3"); // Close any session on line 3
  if (reply == false) Serial.println("Failed to close network session"); // still try to close network service

  reply = sendCommand("AT+NETCLOSE");
  if (reply == false) { Serial.println("Failed to close network session"); return false; }

  return true;
}

// Send a basic test message to a network device
int modemSendUdp(){
  // AT+CIPSEND=<link_num>,<length>,<serverIP>,<serverPort>
  sendData("AT+CIPSEND=3,29,\"85.9.248.158\",420");
  atWait();
  sendData("Hello, Server! This is T-SIM.\n");
  delay(1000); // wait for remote server

  const char* msg = waitForMessageAndRead("+IPD", 12000, /*echo*/false); // wait for server to reply with data
  if (msg == NULL){
    Serial.println("Timeout waiting for server to reply.");
    return false;
  } else {
    Serial.printf("Reply from server >>>\n%s\n<<<\n", msg); // output. Should be byte length, \n\n, reply data
  }

  return true;

  /* // This should not be required, as receive mode is '0' by default, which directly outputs data onto the serial ('COM') port
  // Read buffered result
  const char* result = readCommand("AT+CIPRXGET=2,3"); // ASCII Mode (2) on connection 3
  if (result == NULL){
    Serial.println("Failed to read server reply");
    return false;
  } else {
    Serial.printf("Server replied: >>>\n%s\n<<<\n", result);
    return true;
  }*/

  // AT+CIPRXGET=2,<link_num>    // <-- read all buffered data as ASCII string (raw?)
  // AT+CIPRXGET=3,<link_num>    // <-- read all buffered data as HEX string
  //delay();
}


bool _ctsFlag;
bool _haveTriggeredCommand;
char ewcMsgBuf[128];

void setup() {
 // Connect to USB serial port if available
  Serial.begin(USB_BAUD);
  delay(100);
  Serial.println("Lilygo is up. Program is 06 UDP duplex test.");

  // Output reset types
  RESET_REASON core0 = rtc_get_reset_reason(0);
  RESET_REASON core1 = rtc_get_reset_reason(1);
  print_reset_reason(core0);
  print_reset_reason(core1);

  // if we woke up from deep sleep, don't do anything.
  /*if (core0 == DEEPSLEEP_RESET || core1 == DEEPSLEEP_RESET){
    Serial.println("Woke from deep-sleep. Not starting modem");
    return; // jump to main loop, where we will re-enter deep sleep.
  }*/

  // Connect serial to the SIMCOM module
  /*SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);  // ESP32 <-> SIMCOM
  delay(100);
*/

  // Connect serial to the EWC module
  pinMode(PIN_EWC_TX, OUTPUT);
  pinMode(PIN_EWC_RX, INPUT);
  pinMode(PIN_EWC_CTS, INPUT);
  SerialEWC.begin(EWC_BAUD, SERIAL_8N1, PIN_EWC_RX, PIN_EWC_TX);  // ESP32 <-> EWC

  Serial.print("LEFT THE BIG LOOP");
  delay(250);

  //SerialEWC.println("Hello, EWC");

  // turn the modem on
  /*
  int reply = modemTurnOn();
  if (reply == false) {Serial.println(F("Failed to start SIMCOM modem")); return; }
  
  atWait();
  Serial.println("Modem ready, Attempting UDP exchange");
  atWait();
  reply = modemEnableData();
  if (reply == true){
    Serial.println("Data connection up. Trying to send test message");
    reply = modemSendUdp();
    if (reply == false) Serial.println("Problem sending message");
    reply = modemDisableData();
    if (reply == false) Serial.println("Problem disabling data connection");
    else Serial.println("Data connection down.");
  } else {
    Serial.println("Failed to enable data");
  }
*/
  //SerialEWC.println("Hello, EWC");
/*
  atWait();
  Serial.println("Set-up complete. Going to main loop ");
  */
 _haveTriggeredCommand = false;
}

void loop() {
  // Drop directly into deep sleep

  bool newCtsFlag = !digitalRead(PIN_EWC_CTS);
  if (newCtsFlag && !_ctsFlag){
    Serial.print(" CTS on;");
    _ctsFlag = true;
    if (!_haveTriggeredCommand){ // do this just once
      _haveTriggeredCommand = true;
      // Request EWC clock. Response should be 8054...03xx
      //SerialEWC.write(0x54);
      //SerialEWC.write(0x03);
      //SerialEWC.write(0x57);

      int sttuMsg[12] = {0x4c, 0x00, 0xd4, 0x3d, 0x46, 0xa5, 0x00, 0x00, 0xff, 0xff, 0x03, 0x45}; // super-tap top-up for slot 0
      for (int i = 0; i < 12; i++){
        SerialEWC.write(sttuMsg[i]);
      }
    }
  } else if (!newCtsFlag && _ctsFlag){
    Serial.print(" CTS off;");
    _ctsFlag = false;
  }

  int available = SerialEWC.available();
  if (available > 0){
    int actual = SerialEWC.readBytes(ewcMsgBuf, available);
    //Serial.printf("%d of %d: ", actual, available);
    for (int i = 0; i < actual; i++){
      Serial.printf("%02X", (unsigned char)ewcMsgBuf[i]);
    }
  }

  //SerialEWC.println("Hello, EWC");
/*
  Serial.print("Turning off modem...");
  modemTurnOff();
  atWait();*/
  //Serial.print("Sleeping... Z");
  //esp_sleep_enable_timer_wakeup(ONE_HOUR_S * S_TO_uS);
  //Serial.print("z");
  //delay(200);
  //Serial.print("z");
  //delay(200);
  //esp_deep_sleep_start(); // never returns. We will get reset with DEEPSLEEP_RESET
}