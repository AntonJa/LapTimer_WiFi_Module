WiFi module for LapTimer device
====================

laptimer.freeriders.lv

Сделанно на основе ESP32-wroom-32

Для прошивки ESP:
1. Скачайте последние исходники
'''
   $ git clone https://github.com/AntonJa/LapTimer_WiFi_Module.git
'''
2. перейдите в директорию "LapTimer_WiFi_Module"
'''
   $ cd LapTimer_WiFi_Module
'''
3. Задайте целевое устройство
'''
   $ idf.py set-target esp32
'''
4. Скомилируйре программу
'''
   $ make
'''
5. Включите ESP32-WROOM-32
6. Подключитесь к ESP с помощью USB-RS232 или RS232
7. Введите ESP в режим загрущика
   1. нажмине и удерживайте СБРОС (EN=0в)
   2. нажмине и удерживайте ЗАГРУЗКА (IO0=0в)
8. начните загрузку
'''
   $ make flash
'''
9. можете отпустить СБРОС и ЗАГРУЗКА
10. дождитесь завершения загрузик.

Если подключиться не удалось, повторитье шаги 7..10


Based on ESP32-wroom-32

To flash ESP:
1. get latest code from git
'''
   $ git clone https://github.com/AntonJa/LapTimer_WiFi_Module.git
'''
2. go to dir "LapTimer_WiFi_Module"
'''
   $ cd LapTimer_WiFi_Module
'''
3. set target
'''
   $ idf.py set-target esp32
'''
4. make code
'''
   $ make
'''
5. Power on ESP32-WROOM-32
6. connect to ESP via USB-RS232 either RS232 directly
7. enter bootloader
   1. press and hold Reset (EN=0V)
   2. press and hold BOOT (IO0=0V)
8. start flash ESP
'''
   $ make flash
'''
9. you can release Reset and BOOT, now
10. wait flashing to finish

If connecting failed, repeat steps 7..10
