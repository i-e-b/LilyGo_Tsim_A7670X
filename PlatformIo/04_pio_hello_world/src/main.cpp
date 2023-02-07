
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

// Send data to modem. Don't wait for any reply
void sendData(const char* data) {
  SerialAT.println(data);
  delay(500);
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

  atWait();
  reply = sendCommand("AT+CGATT?");
  if (reply==false) {Serial.println(F("Failed to read packet domain attach or detach status"));return;}


  atWait();
  reply = sendCommand("AT+CGACT=1,1"); // try to attach cid 1? (Page 100)
  if (reply==false) {Serial.println(F("Failed to attach to PDP context"));return;}
  
  // This gives me a failure. Not sure why.
  // Set the "hologram" APN
  // This should be setting up a PDP? https://www.tutorialspoint.com/gprs/gprs_pdp_context.htm   https://en.wikipedia.org/wiki/GPRS_core_network
  //reply = sendCommand("AT+CGDCONT=0,\"IP\",\"hologram\"\n"); // AT+CGDCONT: Define PDP context
  //if (reply==false) {
  //  sendCommand("AT+CEER\r"); // Get detailed error report. See pages 504, 171
  //  Serial.println(F("Failed to set API"));
  //  return;
  //}

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

  // Upload the body data to SIMCOM module
  // TODO: count the length of the string.
  // "AT+HTTPDATA=<size>,<time>" -> DOWNLOAD\n<WRITE DATA TO SIMCOM>\nOK
  sendData("AT+HTTPDATA=52,10\r"); // bytes, time in seconds
  tryReadModem(); // skip over "DOWNLOAD"
  reply = sendCommand("T-SIM message number 2. Hello to you in server land!"); // once we've written enough data, SIMCOM should end the download session by sending "OK"
  if (reply==false) {Serial.println(F("Failed to upload POST body"));return;}

  atWait();

  // Send the request. Note, there are 6xx and 7xx errors the SIMCOM can output. See the datasheet page 322
  // this returns status code and {<method>,<statuscode>,<datalen>}. Example, for a successful get request: +HTTPACTION: 0,200,104220
  reply = sendCommand("AT+HTTPACTION=1"); // 0=GET;1=POST;2=HEAD;3=DELETE;4=PUT
  if (reply==false) {Serial.println(F("Failed to upload POST body"));return;}

  atWait();
  Serial.println("###### SUCCESS! Check the server side to confirm message sent ######");



  // "AT+HTTPREAD?" -> +HTTPREAD: LEN,<len> \n OK
  sendData("AT+HTTPREAD?\r"); // bytes, time in seconds
  const char* replyStr = tryReadModem(); // +HTTPREAD: LEN,<len>\nOK

  if (replyStr == NULL){
    Serial.println("Failure while trying to read reply: reply string was null");
    return;
  } else {
    Serial.print("This is the reply string: [\"");
    Serial.print(replyStr);
    Serial.println("\"] End of reply string.");
  }
  
  char* offs = strstr(replyStr, "LEN,");
  if (offs == NULL){
    Serial.println("Failure while trying to read reply: did not find 'LEN,'");
    return;
  }

  // quick test? TODO: better string handling
  int ilen = 0;
  offs+=4; // skip "LEN,"
  Serial.print("[[");
  for (int i=0; i < 3; i++){
    char c = *offs;
    if (c >= '0' && c <= '9'){
      ilen *= 10;
      ilen += (int)(c-'0');
      Serial.print(c);
    } else {
      Serial.printf(" ended on '%c'", c);
      break;
    }
    offs++;
  }
  Serial.print("]]");

  if (ilen < 1){
    Serial.println("Failed to read data length");
    return;
  }

  char* commandStr;
  if(0 > asprintf(&commandStr, "AT+HTTPREAD=%d", ilen)) {
    Serial.println("Failed to generate read command");
    return;
  }
  Serial.println(&commandStr[0]);

  // "AT+HTTPREAD=<byte_size>" -> OK\n\n<data>\n+HTTPREAD: 0
  reply = sendCommand(&commandStr[0]);
  if (reply==false) {Serial.println(F("Failed to read body"));return;}
  free(commandStr);
  // Reply should be dumped in the console now...?
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