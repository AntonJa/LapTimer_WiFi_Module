#!/bin/sh
/home/anton/ESP32/xtensa-esp32-elf/bin/xtensa-esp32-elf-addr2line -pfiaC -e /home/anton/ESP32/iotbits_esp32/esp-idf/examples/esp32-webserver/build/MGR-2.0.elf "$@"
