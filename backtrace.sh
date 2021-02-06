#!/bin/sh
#/home/anton/ESP32/xtensa-esp32-elf/bin/xtensa-esp32-elf-addr2line -pfiaC -e /home/anton/ESP32/iotbits_esp32/esp-idf/examples/esp32-webserver/build/MGR-2.0.elf "$@"
/home/esp32/.espressif/tools/xtensa-esp32-elf/esp-2020r2-8.2.0/xtensa-esp32-elf/bin/xtensa-esp32-elf-addr2line -pfiaC -e /home/esp32/LapTimer_WiFi_Module/build/LapTimer.elf "$@"
