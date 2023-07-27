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
idf.py -p ESP-PORT -b 115200 monitor  
```

