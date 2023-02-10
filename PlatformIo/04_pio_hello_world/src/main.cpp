
// Real-time clock (used to get reset type)
#include <rom/rtc.h>
// Basic Arduino stuff (Long-term TODO: remove this and do the low level stuff ourself)
#include <Arduino.h>

// System time get/set (ESP32 RTC)
#include "time.h"
#include <sys/time.h>

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

/* // This doesn't seem to be effective.
  reply = sendCommand("AT+CTZU=1"); // Enable updating internal clock from NITZ
  if (reply == true) {
    reply = sendCommand("AT&W"); // store settings
    if (reply == true){
      Serial.println("Wrote NITZ setting");


      // Note, we can do this to force off the network and pick up the date change
      //AT+COPS=2
      //> OK
      //AT+CTZU=1
      //> OK
      //AT+COPS=0
      //> OK
      //+CTZU: "15/05/06,17:25:42",-12,0
      //
    }
  }
*/

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

#define isInt(c) (c >= 0 && c <= 9)
#define notNull(c) (c != 0)

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

void makeHttpCall(char* message) {
  // Start the SIMCOM HTTP(S) Service
  int reply = sendCommand("AT+HTTPINIT");
  if (reply==false) {Serial.println(F("Failed to start HTTP service"));return;}

  // Set parameters for a HTTP call
  reply = sendCommand("AT+HTTPPARA=\"URL\",\"https://tech.ewater.services/Experiments/CellTouch\"");
  if (reply==false) {Serial.println(F("Failed to set URL"));return;}
  reply = sendCommand("AT+HTTPPARA=\"CONTENT\",\"text/plain\"");
  if (reply==false) {Serial.println(F("Failed to set content type"));return;}
  reply = sendCommand("AT+HTTPPARA=\"ACCEPT\",\"*/*\"");
  if (reply==false) {Serial.println(F("Failed to set accept type"));return;}

  int messageBytes = strlen(message);
  if (messageBytes <= 0 || messageBytes > 1048576){ Serial.printf("Invalid outgoing data length: %d\r\n", messageBytes); return; }

  // Upload the body data to SIMCOM module
  // TODO: count the length of the string.
  // "AT+HTTPDATA=<size>,<time>" -> DOWNLOAD\n<WRITE DATA TO SIMCOM>\nOK
  sendDataF("AT+HTTPDATA=%d,10", messageBytes); // bytes, time in seconds
  tryReadModem(); // skip over "DOWNLOAD"
  reply = sendCommand(message); // once we've written enough data, SIMCOM should end the download session by sending "OK"
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

// Read a string, populating an array of ints with each number found.
// Return count of numbers found, or zero in case of errors
// If maxCount is exceeded, the first numbers found are bumped off the back of the list
// so this will return the last maxCount numbers found.
int readNumberSet(const char* src, int maxCount, int* target){
  char* c = (char*)src; // current character
  int idx = 0; // output index
  int tmp = 0; // number being read
  bool inNum = false;

  while (notNull(*c)){
    int i = (int)(*c - '0');
    c++;
    if (isInt(i)){
      if (!inNum){ // starting a new number
        inNum = true;
        tmp = 0;
      }
      tmp = (tmp*10)+i;
    } else {
      if (inNum){ // tmp is complete, write to output
        if (idx >= maxCount){ // push everything back
          for (int i=1; i < maxCount; i++) target[i-1] = target[i];
          idx = maxCount - 1;
        }
        target[idx] = tmp;
        idx++;
      }
      inNum = false;
    }
  }

  if (inNum){ // finish last number
    if (idx >= maxCount) { // push everything back
      for (int i = 1; i < maxCount; i++) target[i - 1] = target[i];
      idx = maxCount - 1;
    }
    target[idx] = tmp;
    idx++;
  }

  return idx;
}


// Turn on and configure the GPS/GNSS system
// It will take around 4 minutes to get a fix if you have attained a fix in the last 4 hours or so
// If you have not got a lock in over 4 hours, it might take 10 minutes to get a fix (ephemeris tables need to be copied from satellite data)
// You can configure the SIMCOM to read ephemeris tables from the network to speed up fixing, and a cost of data use. This is not done here.
// Poll AT+CGPSINFO or AT+CGNSSINFO until you get a valid result.
// Calls to the xINFO commands may fail even after a lock is acheived,
// so make sure you have error detection in place (it is common to get a loss shortly after first lock. Not sure why.)
int activateGPS(){
  delay(1000);

  // turn on power
  int reply = sendCommand("AT+CGNSSPWR=1");
  if (reply==false) {Serial.println(F("Fail: GPS/GNSS power on")); return false; }
  delay(1000);

  // wait for the ready signal
  reply = waitForMessage("+CGNSSPWR: READY!", 12000);
  if (reply==false) {Serial.println(F("GNSS module did not reply within wait period")); return false; }
  Serial.println(F("GNSS module is powered on"));

  //delay(2000);
  //reply = sendCommand("AT+CGNSSTST=1"); // Send data from UART3 to NMEA ... ?
  //if (reply==false) {Serial.println(F("Fail: send data received from UART3 to NMEA port")); return false; }

  delay(1000);
  return true;
}

// Read the ESP32 real-time-clock, and write the
// result the the serial connection.
void readRtc(){
  struct tm local = {0};
  getLocalTime(&local);
  int year = local.tm_year+1900;
  int month = local.tm_mon + 1;

  Serial.printf("%d-%02d-%02d T %02d:%02d:%02d\r\n",year, month, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
}

void setRtcTimeRaw(unsigned long epoch, int ms) {
  struct timeval tv;
  if (epoch > 2082758399){
	  tv.tv_sec = epoch - 2082758399;  // epoch time (seconds)
  } else {
	  tv.tv_sec = epoch;  // epoch time (seconds)
  }
  tv.tv_usec = ms;    // microseconds
  settimeofday(&tv, NULL);
}

// set the internal RTC time
// sc second (0-59); mn minute (0-59); hr hour of day (0-23); dy day of month (1-31);
// mt month (1-12); yr year ie 2021; ms microseconds (optional, pass zero if not known)
void setRtcTime(int sc, int mn, int hr, int dy, int mt, int yr, int ms) {
  // seconds, minute, hour, day, month, year $ microseconds(optional)
  // ie setTime(20, 34, 8, 1, 4, 2021) = 8:34:20 1/4/2021
  struct tm t = {0};        // Initalize to all 0's
  t.tm_year = yr - 1900;    // This is year-1900, so 121 = 2021
  t.tm_mon = mt - 1;
  t.tm_mday = dy;
  t.tm_hour = hr;
  t.tm_min = mn;
  t.tm_sec = sc;
  time_t timeSinceEpoch = mktime(&t);
  setRtcTimeRaw(timeSinceEpoch, ms);
}

int alive = false;

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

  // Print the ESP32 time. Will this be zero after power failure?
  readRtc();

  // Connect serial to the SIMCOM module
  SerialAT.begin(115200, SERIAL_8N1, PIN_RX, PIN_TX);  // ESP32 <-> SIMCOM
  delay(1000);

  // turn the modem on
  int reply = modemTurnOn();
  if (reply == false) {Serial.println(F("Failed to start SIMCOM modem")); return; }
  delay(1000);

  // We could now make HTTP calls
  Serial.print("Modem ready at ");
  readRtc();

  // Test one:
  //makeHttpCall();

  // Wake up the GPS system. It takes ages when it works at all.
  reply = activateGPS();
  if (reply == false) {Serial.println(F("Failed to start GPS sub-system. Reboot modem")); return; }

  // Request CPU temperature reading
  atWait();
  reply = sendCommand("AT+CPMUTEMP");
  if (reply==false) {Serial.println(F("Failed to read SIMCOM CPU temperature"));}

  // Request supply voltage
  atWait();
  reply = sendCommand("AT+CBC");
  if (reply==false) {Serial.println(F("Failed to read SIMCOM supply voltage"));}

  Serial.print("Set-up complete. Going to main loop ");
  alive = true;
  readRtc();
}

int i = 0;
int gotLock = false;      // do we currently have a GPS lock? Get reset if lock is lost
int everHadLock = false;  // have we ever had a lock since power-up?
int gpsData[40];          // we get up to 16 data points, but might read those as two ints
int firstLockMin=0, firstLockSec=0;

void loop() {
    if (!alive){
      Serial.println("System did not start correctly. Will reset NOW.");
      delay(500);
      ESP.restart();
      return;
    }

    atWait();
    i++;
    int mins = (i*4) / 60;
    int hrs  = mins / 60;
    mins = mins % 60;
    int secs = (i*4) % 60;
    Serial.printf("Time since GPS power-up = %02d:%02d:%02d\r\n",hrs, mins,secs);
    if (gotLock){Serial.printf("First lock after = %d:%02d\r\n",firstLockMin,firstLockSec);}

    delay(2000);
  // Read the GNSS position
/*
    const char* gnss = readCommand("AT+CGNSSINFO");
    if (gnss==NULL) {Serial.println(F("Failed to read GNSS location")); }
    else {
      int got = readNumberSet(gnss, 36, gpsData); // NOTE: this reads 12.34 as two integers: 12, and 34
      if (got < 1){
        gotLock = false;
        Serial.println(F("No GNSS data"));
      } else {
        Serial.printf("Found %d datapoints in GNSS: ", got);
        for (int j = 0; j < got && j < 40; j++){
          Serial.printf("%d, ", gpsData[j]);
        }

        long lat_a = gpsData[4];
        long lat_b = gpsData[5];
        long lon_a = gpsData[6];
        long lon_b = gpsData[7];

        long lat_deg = lat_a / 100;
        long lon_deg = lon_a / 100;

        lat_b += (lat_a % 100) * 100000;
        lon_b += (lon_a % 100) * 100000;
        lat_b /= 60;
        lon_b /= 60;

        lon_deg = -lon_deg; // TODO: detect W or E. Do the inversion for W only.

        Serial.printf("\r\nGNSS:  https://www.openstreetmap.org/#map=19/%d.%05d/%d.%05d", lat_deg, lat_b, lon_deg, lon_b);// not correct, due to leading zeros

        Serial.println();
        if (!gotLock){firstLockMin=mins; firstLockSec=secs;}
        gotLock = true;
      }
    }
*/
    delay(1000);

    const char* gps = readCommandQuiet("AT+CGPSINFO");
    if (gps==NULL) {Serial.println(F("Failed to read GPS location")); }
    else {
      int got = readNumberSet(gps, 36, gpsData); // NOTE: this reads 12.34 as two integers: 12, and 34
      if (got < 6){
        gotLock = false;
        Serial.printf("No GPS data (%d)\n", got);
      } else {
        Serial.printf("Found %d datapoints in GPS: ", got);
        for (int j = 0; j < got && j < 40; j++){
          Serial.printf("%d, ", gpsData[j]);
        }

// TODO: pull this out to a function
        long lat_a = gpsData[0];
        long lat_b = gpsData[1];
        long lon_a = gpsData[2];
        long lon_b = gpsData[3];

        long date = gpsData[4];  // like 080223
        long time = gpsData[5];  // like 150151

        if (date == 0){
          Serial.println("Invalid date from GPS. Ignoring.");
          return;
        }

        long hours24 = time / 10000;
        long minutes = (time / 100) % 100;
        long seconds = time % 100;

        long year = (date % 100); // 00..99
        long month= (date / 100) % 100;
        long day  = (date / 10000) % 100;

        // Set time like this: `AT+CCLK="14/01/01,02:14:36+08"`
        // Format is "yy/MM/dd,hh:mm:ss±zz", no optional parts

        // Set ESP32 RTC based on GPS time
        setRtcTime(seconds, minutes, hours24, day, month, year+2000, 0);


        long lat_deg = lat_a / 100;
        long lon_deg = lon_a / 100;

        lat_b += (lat_a % 100) * 100000;
        lon_b += (lon_a % 100) * 100000;
        lat_b /= 60;
        lon_b /= 60;

        lon_deg = -lon_deg; // TODO: detect W or E. Do the inversion for W only?

        Serial.printf("\r\nGPS:  https://www.openstreetmap.org/#map=19/%d.%05d/%d.%05d", lat_deg, lat_b, lon_deg, lon_b);
        Serial.println();

        if (!everHadLock) { // if this is the first lock since start-up, send it back to home server
          // Set SIMCOM clock based on GPS time
          char *setTimeCmd;
          if (0 > asprintf(&setTimeCmd, "AT+CCLK=\"%02d/%02d/%02d,%02d:%02d:%02d+00\"",
                          year, month, day, hours24, minutes, seconds)) {
            Serial.println("Failed to generate SIMCOM clock command");
          } else {
            Serial.println(&setTimeCmd[0]);
            int reply = sendCommand(setTimeCmd);
            if (reply == false) {Serial.println(F("Failed to set SIMCOM clock from GPS time"));}
            else {Serial.println(F("Updated SIMCOM time from GPS"));}
            free(setTimeCmd);
          }

          // Send our acquisition to remote server
          firstLockMin=mins; firstLockSec=secs;
          char *httpMsgStr;
          // Found 11 datapoints in GPS: 5149, 48561, 301, 87739, 80223, 125658, 0, 114, 0, 0, 579, 
          if (0 > asprintf(&httpMsgStr, "T-SIM got a GPS lock. Time=%02d:%02d:%02d; Date=20%02d-%02d-%02d; Location=https://www.openstreetmap.org/#map=19/%d.%05d/%d.%05d",
                          hours24, minutes, seconds, year, month, day, lat_deg, lat_b, lon_deg, lon_b)) {
            Serial.println("Failed to generate HTTP message");
          } else {
            Serial.println(&httpMsgStr[0]);
            makeHttpCall(httpMsgStr); // enable to really send the message
            free(httpMsgStr);
          }
        }
        gotLock = true;
        everHadLock = true;
      }
    }

    delay(1000);

  /*
  // Test is complete Set ESP32 to sleep mode
  Serial.print("Z");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S);
  Serial.print("z");
  delay(200);
  esp_deep_sleep_start(); // never returns. We will get reset with DEEPSLEEP_RESET

  ESP.restart();*/
}