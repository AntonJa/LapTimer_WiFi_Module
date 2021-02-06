#ifndef __STRUCTURES_H__
#define __STRUCTURES_H__

#include "esp_wifi.h"

#define LED_BUILTIN 5

typedef struct timer_mode_conf {
   bool lap_gym;
   uint8_t bikes;
} timer_mode_conf_type;

typedef struct modes_conf {
   uint8_t save;
   timer_mode_conf_type mode[3];
} modes_conf_type;

typedef struct ds3231_conf {
   bool changed;
   bool i2c_busy;
   bool i2c_waiting;
   bool www_busy;
   char date_time_ro[20]; //DD-MM-YYYY hh-mm-ss
   uint8_t bcd_seconds;
   uint8_t bcd_minutes;
   uint8_t bcd_hour;
   bool hour_12h_format; // 0-24h format; 1-12h format;
   uint8_t bcd_week_day;
   uint8_t bcd_date;
   uint8_t bcd_month;
   bool century;  // year overflow 99->00
   uint8_t bcd_year;
   bool A1IE;     // (def-0) Enable A1 alarm
   bool A2IE;     // (def-0) Enable A2 alarm
   bool INTCN;    // (def-1) 0-Square wave on SQW; 1-INT on INT pin
   bool RS1;      // (def-11) RS2-RS1 Square wave frequency
   bool RS2;      // 00-1Hz; 01-1.024kHz; 10-4.096kHz; 11-8.192kHz
   bool CONV;     // if BSY=0 start TCXO
   bool BBSQW;    // (def-0) Enable Square Wavw on Vbat
   bool EOSC;     // (def-0) 0-Run osc on Vbat; 1-Stoposc on Vbat
   bool A1F;      // Alarm 1 flag
   bool A2F;      // Alarm 2 flag
   bool BSY;      // Busy on TCXO
   bool EN32kHz;  // (def-1) Enable pin 32kHz
   bool OSF;      // (def-1) OSC was stopped
   uint8_t aging; // 0.1ppm @ 25C Manual CONV, after chage must be done
   uint16_t temperature; // temperature (step=0.25C)
} ds3231_conf_type;

typedef struct reading_str {
   uint16_t id;
   uint8_t lapGym;
   uint8_t bykeNr;
   uint8_t bykes;
   uint32_t value;
   int referee;
} reading_type;

typedef struct wifi_conf {
//   uint8_t save;
   bool wifi_mode;    // 0=Access Point, 1=WiFi station
   bool wifi_connected;

   //WiFi connection config
   wifi_sta_config_t sta_conf;
//typedef struct {
//    uint8_t ssid[32];      /**< SSID of target AP*/
//    uint8_t password[64];  /**< password of target AP*/
//    wifi_scan_method_t scan_method;    /**< do all channel scan or fast scan */
//    bool bssid_set;        /**< whether set MAC address of target AP or not. Generally, station_config.bssid_set needs to be 0; and it needs to be 1 only when users need to check the MAC address of the AP.*/
//    uint8_t bssid[6];      /**< MAC address of target AP*/
//    uint8_t channel;       /**< channel of target AP. Set to 1~13 to scan starting from the specified channel before connecting to AP. If the channel of AP is unknown, set it to 0.*/
//    uint16_t listen_interval;   /**< Listen interval for ESP32 station to receive beacon when WIFI_PS_MAX_MODEM is set. Units: AP beacon intervals. Defaults to 3 if set to 0. */
//    wifi_sort_method_t sort_method;    /**< sort the connect AP in the list by rssi or security mode */
//    wifi_scan_threshold_t  threshold;     /**< When sort_method is set, only APs which have an auth mode that is more secure than the selected auth mode and a signal stronger than the minimum RSSI will be used. */
//    wifi_pmf_config_t pmf_cfg;    /**< Configuration for Protected Management Frame. Will be advertized in RSN Capabilities in RSN IE. */
//} wifi_sta_config_t;

   uint8_t ap_ip[4];
   uint8_t ap_mask[4];
   uint8_t ap_gw[4];
   //Access point config
   wifi_ap_config_t ap_conf;
//typedef struct {
//    uint8_t ssid[32];           /**< SSID of ESP32 soft-AP */
//    uint8_t password[64];       /**< Password of ESP32 soft-AP */
//    uint8_t ssid_len;           /**< Length of SSID. If softap_config.ssid_len==0, check the SSID until there is a termination character; otherwise, set the SSID length according to softap_config.ssid_len. */
//    uint8_t channel;            /**< Channel of ESP32 soft-AP */
//    wifi_auth_mode_t authmode;  /**< Auth mode of ESP32 soft-AP. Do not support AUTH_WEP in soft-AP mode */
//    uint8_t ssid_hidden;        /**< Broadcast SSID or not, default 0, broadcast the SSID */
//    uint8_t max_connection;     /**< Max number of stations allowed to connect in, default 4, max 4 */
//    uint16_t beacon_interval;   /**< Beacon interval, 100 ~ 60000 ms, default 100 ms */
//} wifi_ap_config_t;
} wifi_conf_type;

//int reading_cnt;

typedef struct{
    reading_type* list;
    int len;
    int8_t timerStep;
} readings_struct;

readings_struct readings;

reading_type *test[2];

typedef struct setup_conf {
   bool user_active;
   bool init; // init done
   ds3231_conf_type ds_conf;
   wifi_conf_type wifi_conf;
   modes_conf_type modes;
} setup_type;


#endif /* __STRUCTURES_H__ */
