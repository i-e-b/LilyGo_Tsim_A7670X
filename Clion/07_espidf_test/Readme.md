ESP-IDF under CLion
===================

See https://github.com/i-e-b/esp-idf-clion

When starting work on the project (after IDE restart)
-----------------------------------------------------

- Set up esp-idf environment variables Open Clion Terminal, and running
```
. ~/esp/esp-idf/export.sh
```

Initial setup
-------------

- Set up project In CLion terminal running :
```
idf.py set-target esp32
idf.py menuconfig
```

(`menuconfig` is used for configuration of esp-idf)

Manual build/deploy
-------------------

- Build esp project In CLion terminal running :
```
idf.py build
```

- Flash
```
idf.py -p ESP-PORT -b 115200 flash
```

- Monitor
```
idf.py monitor -b 115200 -p /dev/ttyUSB0
```

Lilygo log output

```
rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7132
ho 0 tail 12 room 4
load:0x40078000,len:15708
load:0x40080400,len:4
ho 8 tail 4 room 4
load:0x40080404,len:3884
entry 0x40080650
I (32) boot: ESP-IDF v5.2-dev-1890-g28167ea5a3-dirty 2nd stage bootloader
I (32) boot: compile time Jul 27 2023 13:05:42
I (35) boot: Multicore bootloader
I (39) boot: chip revision: v3.0
I (43) boot.esp32: SPI Speed      : 40MHz
I (48) boot.esp32: SPI Mode       : DIO
I (52) boot.esp32: SPI Flash Size : 4MB
I (57) boot: Enabling RNG early entropy source...
I (62) boot: Partition Table:
I (66) boot: ## Label            Usage          Type ST Offset   Length
I (73) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (80) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (88) boot:  2 factory          factory app      00 00 00010000 00100000
I (95) boot: End of partition table
I (100) esp_image: segment 0: paddr=00010020 vaddr=3f400020 size=09718h ( 38680) map
I (122) esp_image: segment 1: paddr=00019740 vaddr=3ffb0000 size=020f0h (  8432) load
I (126) esp_image: segment 2: paddr=0001b838 vaddr=40080000 size=047e0h ( 18400) load
I (136) esp_image: segment 3: paddr=00020020 vaddr=400d0020 size=13a00h ( 80384) map
I (165) esp_image: segment 4: paddr=00033a28 vaddr=400847e0 size=0780ch ( 30732) load
I (185) boot: Loaded app from partition at offset 0x10000
I (185) boot: Disabling RNG early entropy source...
I (196) cpu_start: Multicore app
I (196) cpu_start: Pro cpu up.
I (197) cpu_start: Starting app cpu, entry point is 0x40081030
I (183) cpu_start: App cpu up.
I (215) cpu_start: Pro cpu start user code
I (215) cpu_start: cpu freq: 160000000 Hz
I (215) cpu_start: Application information:
I (219) cpu_start: Project name:     07_espidf_test
I (225) cpu_start: App version:      3bf31f6-dirty
I (230) cpu_start: Compile time:     Jul 27 2023 13:05:55
I (237) cpu_start: ELF file SHA256:  29c105e6a...
I (242) cpu_start: ESP-IDF:          v5.2-dev-1890-g28167ea5a3-dirty
I (249) cpu_start: Min chip rev:     v0.0
I (254) cpu_start: Max chip rev:     v3.99 
I (258) cpu_start: Chip rev:         v3.0
I (263) heap_init: Initializing. RAM available for dynamic allocation:
I (271) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (276) heap_init: At 3FFB2960 len 0002D6A0 (181 KiB): DRAM
I (283) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (289) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (296) heap_init: At 4008BFEC len 00014014 (80 KiB): IRAM
I (303) spi_flash: detected chip: generic
I (306) spi_flash: flash io: dio
I (311) app_start: Starting scheduler on CPU0
I (315) app_start: Starting scheduler on CPU1
I (315) main_task: Started on CPU0
I (325) main_task: Calling app_main()
Hello world! This is a basic ESP-IDF test.
This is esp32 chip with 2 CPU core(s), WiFi/BTBLE, silicon revision v3.0, 4MB external flash
Minimum free heap size: 301244 bytes
```
