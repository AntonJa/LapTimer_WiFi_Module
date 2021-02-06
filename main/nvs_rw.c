#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "fn_defs.h"
#include "structures.h"
#include "string.h"
#include "driver/gpio.h"

#define PART_STR "NVS: "
#define LOG(str, ... ) printf(PART_STR);\
                       printf(str, ##__VA_ARGS__)

static nvs_handle_t nvs_wifi_handle;
static esp_err_t err;
#define WIFI_STORAGE_NAMESPACE "WiFi"
setup_type *Setup;
bool handle_ready;

esp_err_t safe_open(){
    if ( ! handle_ready){
        err = nvs_open(WIFI_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_wifi_handle);
        if (err != ESP_OK){
            LOG("Error (%d) opening NVS!\n", err);
            return err;    
        }
        handle_ready = true;
    }
    return ESP_OK;
}

void safe_close(){
    if (handle_ready){
        nvs_close(nvs_wifi_handle);
        handle_ready = false;
    }
}

esp_err_t nvs_set_wifi_conf(){
    // Open
    err = safe_open();
    if (err != ESP_OK) return err;

    // Write
    LOG("Set Blob\n");
    err = nvs_set_blob(nvs_wifi_handle, "nvs_wifi_conf", 
                       &Setup->wifi_conf, sizeof(Setup->wifi_conf));
    if (err != ESP_OK) return err;
    err = nvs_commit(nvs_wifi_handle);
    if (err != ESP_OK) return err;    

    // Close
    safe_close();
    return ESP_OK;
    LOG("Set Blob done\n");
}

esp_err_t nvs_reset_wifi_conf(){
    #ifndef CONFIG_WIFI_MODE // 0=Access Point, 1=WiFi station
    Setup->wifi_conf.wifi_mode = false;
    #else
    Setup->wifi_conf.wifi_mode = true;
    #endif

    #ifndef CONFIG_AP_SSID_HIDDEN
    Setup->wifi_conf.ap_conf.ssid_hidden = 0;
    #else
    Setup->wifi_conf.ap_conf.ssid_hidden = 1;
    #endif
    Setup->wifi_conf.ap_conf.authmode = CONFIG_AP_AUTHMODE;
    Setup->wifi_conf.ap_conf.max_connection = CONFIG_AP_MAX_CONNECTION;
    strcpy((char*)Setup->wifi_conf.ap_conf.ssid, CONFIG_AP_SSID);
    strcpy((char*)Setup->wifi_conf.ap_conf.password, CONFIG_AP_PASS);
    Setup->wifi_conf.ap_conf.channel = CONFIG_AP_CHANNEL;
    Setup->wifi_conf.ap_conf.beacon_interval = CONFIG_AP_BEACON_INTERVAL;

    Setup->wifi_conf.ap_ip[0] = 192;
    Setup->wifi_conf.ap_ip[1] = 168;
    Setup->wifi_conf.ap_ip[2] = 0;
    Setup->wifi_conf.ap_ip[3] = 1;
    Setup->wifi_conf.ap_mask[0] = 255;
    Setup->wifi_conf.ap_mask[1] = 255;
    Setup->wifi_conf.ap_mask[2] = 255;
    Setup->wifi_conf.ap_mask[3] = 0;
    Setup->wifi_conf.ap_gw[0] = 192;
    Setup->wifi_conf.ap_gw[1] = 168;
    Setup->wifi_conf.ap_gw[2] = 0;
    Setup->wifi_conf.ap_gw[3] = 1;

    strcpy((char*)Setup->wifi_conf.sta_conf.ssid, CONFIG_STA_SSID);
    strcpy((char*)Setup->wifi_conf.sta_conf.password, CONFIG_STA_PASS);
    Setup->wifi_conf.sta_conf.bssid[0] = 0;
    Setup->wifi_conf.sta_conf.bssid[1] = 0;
    Setup->wifi_conf.sta_conf.bssid[2] = 0;
    Setup->wifi_conf.sta_conf.bssid[3] = 0;
    Setup->wifi_conf.sta_conf.bssid[4] = 0;
    Setup->wifi_conf.sta_conf.bssid[5] = 0;
    #ifndef CONFIG_STA_BSSID_SET
    Setup->wifi_conf.sta_conf.bssid_set = false;
    #else
    Setup->wifi_conf.sta_conf.bssid_set = true;
    #endif

    err = nvs_set_wifi_conf();
    if(err != ESP_OK){
         LOG("Error (%s)\n", esp_err_to_name(err));
    }
    return ESP_OK;
}

esp_err_t nvs_get_wifi_conf(bool reset){
    LOG("Get start\n");
    // Open
    err = safe_open();
    if (err != ESP_OK) return err;

    LOG("Get Blob size\n");
    // Read
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(nvs_wifi_handle, "nvs_wifi_conf", NULL, &required_size);
    LOG("Req size=%d, Blob size=%d\n", sizeof(wifi_conf_type), required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    if (required_size != sizeof(wifi_conf_type) && required_size != 0){
        LOG("Overwriting\n");
	nvs_reset_wifi_conf();
        return ESP_OK;
    }

    LOG("Get blob data\n");
    // Read previously saved blob if available
    if (required_size > 0 && reset == false) {
        LOG("Blob exists\n");
        err = nvs_get_blob(nvs_wifi_handle, "nvs_wifi_conf", &Setup->wifi_conf, &required_size);
        if (err != ESP_OK) return err;
	Setup->wifi_conf.wifi_connected = false;
    }else{
        LOG("Blob NOT found\n");
        nvs_reset_wifi_conf();
    }

    LOG("Close blob\n");
    safe_close();
    return ESP_OK;
}

void nvs_rw_init(void * parSetup){
    Setup = parSetup;
    err = nvs_get_wifi_conf(false);
    if(err != ESP_OK){
        LOG("Error (%s)\n", esp_err_to_name(err));
    }
}
