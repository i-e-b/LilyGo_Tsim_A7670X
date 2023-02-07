# LilyGo_Tsim_A7670X
T-SIM A7670 E development setup, tests, and notes

## General notes

None of the built-in LEDs can be directly flashed by the ESP32 CPU.
* The red LEDs are under control of the SIMCOM module. They are `NETLIGHT` and `STATUS`.
* The blue LEDs are under control of the CN3065 charge controller. They are `CHRG` and `STDBY`.

The *Table of Contents* in the SIMCOM datasheet starts page 20.

## SIM setup

I am using a SIM card from https://hologram.io/ , with the Pilot (1MB) plan.
The dashboard is at https://dashboard.hologram.io/

Before trying to connect, your SIM should be 'activated'.
See https://dashboard.hologram.io/org/{my-org}/device/{my-device}/status?page=1
You should get the message:

```
Your device is activated and ready for use on our network. We have not yet seen it connect to a local tower or pass data.
```

# Arduino setup

Manufacturer's instructions are at: https://github.com/Xinyuan-LilyGO/T-A7670X

* Get Arduino IDE from https://www.arduino.cc/en/software
  * Under Linux (Xubuntu), I got the AppImage. Show properties, allow execute as program
* File -> Preferences; In `Additional boards manager URLS`, add this: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json `. If there are other URLs in here, separate with comma (`,`).
    * This is Espressif's package bundle, for the ESP32 chip (the main CPU)

* Tools -> Boards -> Board Manager... 
    * small box `Filter your search`. Type `esp32`
    * find `esp32` by Espressif Systems (version 2.0.6 as of writing)
    * press install. This will download a whole bunch of packages, and takes a while.
    * ends with `Configuring platform. Platform esp32:esp32@2.0.6 installed` in the 'Output' area.

(note- when you press 'Install' in the Arduino UI, it shows the previous version immediately after starting. This seems to be normal, and the correct version is displayed in output)

* Sketch -> Include Library -> Manage Libraries
    * `TinyGSM` by Volodymyr Shymanskyy (0.11.5)
    * `StreamDebugger` by Volodymyr Shymanskyy (1.0.1)
    * `ArduinoHttpClient` by Arduino (0.4.0; LilyGo recommend https://github.com/ricemices/ArduinoHttpClient ; this README goes with the official Arduino library at https://github.com/arduino-libraries/ArduinoHttpClient . Both options are derived from the same source: https://github.com/amcewen/HttpClient )

* Close and re-open Arduino.
* Plug in the board using USB-C (the USB micro is for the SIMCOM modem only)
    * You should get a bright blue light at the centre of the main board immediately
    * a pair of red lights by the SIMCOM should come up a few seconds later
    * one of the red lights should start flashing after 10-20 seconds
    * once the red light is flashing, the Hologram dashboard should update with:
        `SIM -- LOCAL TOWER -- GLOBAL NETWORK . . INTERNET ACCESS`
        `Your device is currently connected to our network, or has connected recently to our network and has an open data session`

* Tools -> Board -> esp32 -> ESP32 Wrover Module
* Tools -> Get Board Info...
    * If this reports "Please select a port to obtain board info", your connection is not working.
        See "If you can't connect to the board" below.


## Programming

Make sure everything is powered up and is as described in the notes above

### 01 First program: Local test

The first program is just here to check we can connect the ESP32 module, run a program on it, and get the result back to the host PC.

`01_HelloWorld.ino`

```c
#include "Arduino.h"

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Hello, world!");
}

int i = 0;
void loop() {
  delay(1000);
  Serial.printf("Device is active (%d) \r\n", i++);
}
```

* Verify this. There should be no errors.
* Switch to Serial Monitor ( Tools -> Serial Monitor ), and ensure the '... baud' drop-down menu says *exactly* `115200 baud`.
* Press the upload button, or Sketch -> Upload
* There should be some compiler output. Any errors or red text, see Errors section below
* Switch to Serial Monitor. You should see `Device is active (...)` with numbers increasing every second.

Note: The red LEDs next to the SIMCOM module will go out when the device resets after programming.
The default program which wakes up the modem will be overwritten by our test code. We will get it
back soon.

If you see the correct output, move on to next test program.

### 02 Ping the modem

This program tests the connection between the ESP32 (main CPU module) and the SIMCOM (cellular modem module)

`02_HelloModem.ino`

Upload and check the serial monitor. You should get the message `The SIMCOM A7670 Module replied OK to attention command. Looks good.` after some test data.

### 03 Modem library

This program uses the built-in SIMCOM HTTP client to contact an external server, and send&receive data.

`03_modem_library.ino`

**Before running:** Check the constants at the top of the program file.
Upload and run. Each time the program is run, the API will be called.
Check the HTTP server to make sure the call is correctly formatted.
The Serial console should be written with the response from the HTTP server.

This will use some of your data allocation.

## Errors

### Junk in Serial Monitor

**Diagnosis:** scrabled output in serial monitor in the Arduino IDE. Looks like 
```
RBa#�!.ʭ␁����ń�!�i��B�sV҆��*␘F�TRB�Cac�I�!C����9�
```
or similar junk.

**Cause:** Wrong 'baud' rate.

**Fix:** Make sure that your code `Serial.begin()` and the 'baud' drop down in the serial monitor have the identical value.
The serial monitor does **not** update existing data when the drop-down is changed.
Press the `RST` (reset) button on the device once settings are correct.

### No permission to write to USB

**Diagnosis:** `A fatal error occurred: Could not open /dev/{your port}, the port doesn't exist` when you try to upload.

**Cause:** User does not have permission to write to USB serial (Linux)

**Resource:** https://askubuntu.com/questions/899374/arduino-only-works-in-root

**Fix:** Add yourself to the `dialout` group, or add a `udev` rule.

```
sudo usermod -a -G dialout $USER
```
(note: this must be run with `sudo` when logged in as the normal user, and **not** as the root user)

Now either log-out or reboot machine (restarting Arduino doesn't work?)

If you try to write a `udev` rule, `hardinfo` will give you the vendor and product IDs

### Protocol error

**Diagnosis:** `OSError: [Errno 71] Protocol error` when you try to upload code

**Cause:** Wrong USB device, or wrong board type.

**Fix:**
Try unplugging board, and see what port disappears. That's the right port.

If still broken, might be a faulty USB cable, RF interference or magnets near the cable, or some other problem.

Try running `dmesg` or `journalctl -k` while doing an upload, look for any USB issues.

### No module named 'serial'

**Diagnosis:** `ModuleNotFoundError: No module named 'serial'` when you try to verify the code.

**Cause:** Missing Python modules (python is used in cross-compiler workflow)

**Fix:**

In a terminal:
```
pip3 install pyserial
```

if you get an error that `pip3` is not found, install Python version 3.
On Linux:

```
sudo apt install python3-pip
```

### If you can't connect to the board

**Diagnosis:** At the bottom-right of the Arduino window, it says
```
ESP32 Wrover Module [not connected]
```

**Cause:** The board is not connected, or there is a fault in the USB system.

**Resources:** https://support.arduino.cc/hc/en-us/articles/4412955149586-If-your-board-is-not-detected-by-Arduino-IDE

`sudo apt install hardinfo`
`sudo hardinfo`
Devices -> USB Devices
Look for unknown.
If nothing, USB cable might be damaged, or a bad usb port on PC?
When working, it should have `QinHeng Electronics USB Single Serial`

**Fix:** Test device in different USB ports, avoid using a hub. Try different cables.

# VSCode + Platform IO set-up

* The "PlatformIO" IDE installer is at: https://platformio.org/platformio-ide
    * this is VSCode with a plug-in
    * Installed from deb package at https://code.visualstudio.com/docs/?dv=linux64_deb (main download page at https://code.visualstudio.com/ )

In VSCode:
* File -> Preferences -> Extensions
* Search for `PlatformIO` and install
* Once installed, it should show a screen with a button "Install PlatformIO Core". Press this button.
* When this is finished, it will prompt you to restart VSCode. Do this.
* You should have a new icon-button on the left of VSCode window. Looks like an Alien head.
    * This should have a link to "PIO Home", where you can do more set-up and create or load projects.

* The PlatformIO tools will be using the Arduino compiler tooling under the hood, so make sure the Arduino setup is done
* Make sure you create at least one project before loading existing ones.
    * The first project creation takes a while, and will download the required tooling.

When creating a new project:
- **Name:** the new project name. Use characters [a-z0-9_-], no spaces or caps. PlatformIO can be fussy.
- **Board:** `Espressif ESP-WROVER-KIT`
- **Framework:** Arduino
- **Location:** Storage location. Use characters [a-z0-9_-], no spaces or caps. PlatformIO can be fussy.

## Building and uploading

* Open the PlatformIO try, using the Alien head icon
* Under `Project Tasks` > `esp32dev` > `General`
    * Find the lines `Build` and `Upload and Monitor`
    * Press either of these line once to trigger the action

## Code formatting

Other than the various settings and library documents, the code is the same between *Arduino IDE* and *PlatformIO IDE*.

# CLion + Platform IO set-up

Manufacturer's instructions are at: https://github.com/Xinyuan-LilyGO/T-A7670X

* Make sure that CLion is installed (using JetBrains Toolbox, or via https://www.jetbrains.com/clion/ )
    * Add the PlatformIo plugin ( https://plugins.jetbrains.com/plugin/13922-platformio-for-clion )
        * (there is also https://plugins.jetbrains.com/plugin/11284-openocd--esp32-support-for-embedded-development , but not looking at that now )
    * PlatformIO CLion docs are at: https://docs.platformio.org/en/latest/integration/ide/clion.html
    * We will be using the Arduino compiler tooling for CLion projects, so make sure the Arduino setup is done

* To create a PlatformIO project
    1. In CLion, File -> New -> Project...
    2. Select [Embedded] PlatformIO

**NOTE:** The CLion integration for PlatformIO seems to be full of issues and work-arounds.
https://docs.platformio.org/en/latest/integration/ide/clion.html

Going to try the VSCode integration before looking into CLion.

