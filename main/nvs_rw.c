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

static nvs_handle nvs_wifi_handle;
static esp_err_t err;
static wifi_conf_type* nvs_wifi_conf;

esp_err_t nvs_set_wifi_conf(bool handle_ready){
    // Open
    if (handle_ready != true){
        err = nvs_open("wifi", NVS_READWRITE, &nvs_wifi_handle);
        if (err != ESP_OK)
            printf("Error (%d) opening NVS!\n", err);
    } else {
        // Write
        err = nvs_set_blob(nvs_wifi_handle, "nvs_wifi_conf", nvs_wifi_conf, sizeof(nvs_wifi_conf));
        if (err != ESP_OK) return err;
        
        // Close
        if (handle_ready != true)
            nvs_close(nvs_wifi_handle);
    }
    return ESP_OK;
}

esp_err_t nvs_get_wifi_conf(bool reset){
    // Open
    err = nvs_open("wifi", NVS_READWRITE, &nvs_wifi_handle);
    if (err != ESP_OK) {
        printf("Error (%d) opening NVS!\n", err);
    } else {
        // Read
        size_t required_size = 0;  // value will default to 0, if not set yet in NVS
        err = nvs_get_blob(nvs_wifi_handle, "nvs_wifi_conf", NULL, &required_size);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
        if (required_size != sizeof(wifi_conf_type) && required_size != 0) 
            return ESP_ERR_NVS_INVALID_LENGTH;

        // Read previously saved blob if available
        nvs_wifi_conf = malloc(sizeof(wifi_conf_type));
        if (required_size > 0 && reset == false) {
            err = nvs_get_blob(nvs_wifi_handle, "nvs_wifi_conf", nvs_wifi_conf, &required_size);
            if (err != ESP_OK) return err;
        }
        else{
            #ifndef CONFIG_WIFI_MODE
            nvs_wifi_conf->wifi_mode = false;
            #else
            nvs_wifi_conf->wifi_mode = true;
            #endif
            strcpy((char*)nvs_wifi_conf->ap_conf.ssid, CONFIG_AP_SSID);
            strcpy((char*)nvs_wifi_conf->ap_conf.password, CONFIG_AP_PASS);
            nvs_wifi_conf->ap_conf.ssid_len = 0;
            nvs_wifi_conf->ap_conf.channel = CONFIG_AP_CHANNEL;
            nvs_wifi_conf->ap_conf.authmode = CONFIG_AP_AUTHMODE;
            #ifndef CONFIG_AP_SSID_HIDDEN
            nvs_wifi_conf->ap_conf.ssid_hidden = 0;
            #else
            nvs_wifi_conf->ap_conf.ssid_hidden = 1;
            #endif
            nvs_wifi_conf->ap_conf.max_connection = CONFIG_AP_MAX_CONNECTION;
            nvs_wifi_conf->ap_conf.beacon_interval = CONFIG_AP_BEACON_INTERVAL;

            strcpy((char*)nvs_wifi_conf->sta_conf.ssid, CONFIG_STA_SSID);
            strcpy((char*)nvs_wifi_conf->sta_conf.password, CONFIG_STA_PASS);
            #ifndef CONFIG_STA_BSSID_SET
            nvs_wifi_conf->sta_conf.bssid_set = false;
            #else
            nvs_wifi_conf->sta_conf.bssid_set = true;
            #endif
            strcpy((char*)nvs_wifi_conf->sta_conf.bssid, CONFIG_STA_BSSID);

            nvs_set_wifi_conf(true);
        }
        
        // Close
        nvs_close(nvs_wifi_handle);
    }
    return ESP_OK;
}

void nvs_rw_init()
{
    nvs_flash_init();
    
    // Read "reset to defaults" gpio and read
    gpio_pad_select_gpio(CONFIG_RESET_GPIO);
    gpio_set_direction(CONFIG_RESET_GPIO, GPIO_MODE_INPUT);
    nvs_get_wifi_conf(gpio_get_level(CONFIG_RESET_GPIO));
}
