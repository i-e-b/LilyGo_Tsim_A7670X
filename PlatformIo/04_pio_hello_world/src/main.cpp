
// Real-time clock (used to get reset type)
#include <rom/rtc.h>
// Basic Arduino stuff (Long-term TODO: remove this and do the low level stuff ourself)
#include <Arduino.h>

// Message we will send to the server
const char* messageString = "This is a message from the T-SIM micro-controller. It is 🤓 Awesome !";

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
  delay(250);
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
        delay(500);
        if (r.indexOf("OK") >= 0) {
          return true;
        }
        else if (r.indexOf("ERROR") >= 0){
          //delete &r; // ???
          return false; // failed
        }
      }
    }
    i--; // No 'OK', so try again
  }
  delay(500);
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
      Serial.println(r);
      return r.c_str();
    }
  }
  return NULL;
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

  Serial.println(F("Modem power-up starting\r\n"));
}

int isInt(int c){return c >= 0 && c <= 9;}
int notNull(char c){return c != 0;}

// Unpack the reply string from a HTTP action and output the status code and data length
int readHttpActionResult(const char* replyStr, int* statusCode, int* dataLength) {
  char* c = (char*)replyStr;
  int sc = 0; // status code
  int dl = 0; // data length
  bool inNum = false;

  while (notNull(*c)){
    int i = (int)(*c - '0');
    c++;
    if (isInt(i)){
      if (!inNum){
        inNum = true;
        sc = dl;
        dl = 0;
      }
      dl = (dl*10)+i;
    } else {
      inNum = false;
    }
  }

  *statusCode = sc;
  *dataLength = dl;

  return true;
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
  modemTurnOn();
  //delay(1000);

  // test with an 'AT' command
  Serial.println(F("Testing Modem Response..."));
  int reply = sendCommand("AT");
  atWait();
  if (reply==false) {Serial.println(F("** Failed to connect to the modem! Check the baud and try again.**"));return;}
  Serial.println(F("Modem is active and ready"));

  // Start the SIMCOM HTTP(S) Service
  reply = sendCommand("AT+HTTPINIT");
  if (reply==false) {Serial.println(F("Failed to start HTTP service"));return;}

  // Set parameters for a HTTP call
  reply = sendCommand("AT+HTTPPARA=\"URL\",\"https://tech.ewater.services/Experiments/CellTouch\"");
  if (reply==false) {Serial.println(F("Failed to set URL"));return;}
  reply = sendCommand("AT+HTTPPARA=\"CONTENT\",\"text/plain\"");
  if (reply==false) {Serial.println(F("Failed to set content type"));return;}
  reply = sendCommand("AT+HTTPPARA=\"ACCEPT\",\"*/*\"");
  if (reply==false) {Serial.println(F("Failed to set accept type"));return;}

  int messageBytes = strlen(messageString);
  if (messageBytes <= 0 || messageBytes > 1048576){ Serial.printf("Invalid outgoing data length: %d\r\n", messageBytes); return; }

  // Upload the body data to SIMCOM module
  // TODO: count the length of the string.
  // "AT+HTTPDATA=<size>,<time>" -> DOWNLOAD\n<WRITE DATA TO SIMCOM>\nOK
  sendDataF("AT+HTTPDATA=%d,10", messageBytes); // bytes, time in seconds
  tryReadModem(); // skip over "DOWNLOAD"
  reply = sendCommand(messageString); // once we've written enough data, SIMCOM should end the download session by sending "OK"
  if (reply==false) {Serial.println(F("Failed to upload POST body"));return;}

  atWait();

  // Send the request. Note, there are 6xx and 7xx errors the SIMCOM can output. See the datasheet page 322
  // this returns status code and {<method>,<statuscode>,<datalen>}. Example, for a successful get request: +HTTPACTION: 0,200,104220
  sendData("AT+HTTPACTION=1"); // 0=GET;1=POST;2=HEAD;3=DELETE;4=PUT

  const char* replyStr = tryReadModem(); // +HTTPACTION: 1,200,68

  if (replyStr == NULL) { Serial.println(F("Failed to upload POST body")); return; }
  int statusCode = 0;
  int dataLength = 0;
  reply = readHttpActionResult(replyStr, &statusCode, &dataLength);
  atWait();
  if (reply==false) {Serial.println(F("Failed to read action result"));return;}

  if (statusCode < 200 || statusCode > 299){Serial.printf("Non-success status code: %d\r\n", statusCode); return; }
  if (dataLength <= 0 || dataLength > 1048576){ Serial.printf("Invalid data length: %d\r\n", dataLength); return; }
  atWait();
  Serial.println("###### SUCCESS! Check the server side to confirm message sent ######");
  atWait();
  Serial.println("###### Reading response message... ######");


  char* commandStr;
  if(0 > asprintf(&commandStr, "AT+HTTPREAD=%d", dataLength)) {
    Serial.println("Failed to generate read command");
    return;
  }
  Serial.println(&commandStr[0]);

  // "AT+HTTPREAD=<byte_size>" -> OK\n\n<data>\n+HTTPREAD: 0
  reply = sendCommand(&commandStr[0]);
  if (reply==false) {Serial.println(F("Failed to read body"));return;}
  free(commandStr);
  // Reply should be dumped in the console now...?

  // Close the SIMCOM HTTP(S) Service
  reply = sendCommand("AT+HTTPTERM");
  if (reply==false) {Serial.println(F("Http client shut-down failed"));return;}
}

int i = 0;
void loop() {
  sendCommand("AT+CPOF"); // try to power-off the SIMCOM module.
  atWait();

  // Test is complete Set ESP32 to sleep mode
  Serial.print("Z");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S);
  Serial.print("z");
  delay(200);
  esp_deep_sleep_start(); // never returns. We will get reset with DEEPSLEEP_RESET

  ESP.restart();
}