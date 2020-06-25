#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
//#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "freertos/portmacro.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
//#include "tcpip_adapter.h"
#include "esp_netif.h"
#include "fn_defs.h"
#include "structures.h"
#include "driver/spi_slave.h"
#include "lwip/err.h"
#include "string.h"

#define delay(ms) (vTaskDelay(ms/portTICK_RATE_MS))
setup_type setup;

#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"

static int s_retry_num = 0;
static const char *TAG = "wifi";

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        // User connected to AP
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d\n",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d\n",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 5) {
            esp_wifi_connect();
            //xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP\n");
        }
        ESP_LOGI(TAG,"connect to the AP fail\n");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // Connected to external AP
        //ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
	//ESP_LOGI(TAG, "got ip:%s\n",
        //         ip4addr_ntoa(&event->ip_info.ip));

	ip4_addr_t* event = (ip4_addr_t*) event_data;
        ESP_LOGI(TAG, "got ip:%s\n",
                 ip4addr_ntoa(event));
        s_retry_num = 0;
        setup.wifi_conf.save = 7;
    }
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    tcpip_adapter_ip_info_t ipInfo = {0,};
    IP4_ADDR(&ipInfo.ip, setup.wifi_conf.ap_ip[0],setup.wifi_conf.ap_ip[1],setup.wifi_conf.ap_ip[2],setup.wifi_conf.ap_ip[3]);
    IP4_ADDR(&ipInfo.gw, setup.wifi_conf.ap_gw[0],setup.wifi_conf.ap_gw[1],setup.wifi_conf.ap_gw[2],setup.wifi_conf.ap_gw[3]);
    IP4_ADDR(&ipInfo.netmask, setup.wifi_conf.ap_mask[0],setup.wifi_conf.ap_mask[1],
                              setup.wifi_conf.ap_mask[2],setup.wifi_conf.ap_mask[3]);

    tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
    tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ipInfo);
    tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    if (setup.wifi_conf.wifi_mode){ // 0=Access Point, 1=WiFi station
        wifi_config_t wifi_config = {
            .sta = setup.wifi_conf.sta_conf,
        };
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
        ESP_ERROR_CHECK(esp_wifi_start() );

        ESP_LOGI(TAG, "wifi_init_sta finished.\n");
        ESP_LOGI(TAG, "connect to ap SSID:%s password:%s\n",
                 wifi_config.sta.ssid, wifi_config.sta.password);
    }
    else{
        wifi_config_t wifi_config = {
            .ap = setup.wifi_conf.ap_conf,
        };

        printf("main: SSID = %s\n", wifi_config.ap.ssid);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s\n",
                 wifi_config.ap.ssid, wifi_config.ap.password);
        setup.wifi_conf.save = 7;
    }
}

int app_main(void)
{
    nvs_flash_init();
    //system_init();
    sys_init();
    nvs_rw_init();
    readings.len = 0;
    
    gpio_pad_select_gpio(LED_BUILTIN);

    /* Set the GPIO as a push/pull output */
    gpio_set_direction(LED_BUILTIN, GPIO_MODE_OUTPUT);
    xTaskCreate(&spi_slave,  //Function
		"spi_slave", //Name (for debug)
		2048, 	     //Stack deph
		&setup,	     //Param's to pass to the task
		5, 	     //Priority
		NULL);	     //Task handle
    xTaskCreate(&i2c_master,  //Function
		"i2c_master", //Name (for debug)
		2048, 	      //Stack deph
		&setup.ds_conf, //Param's to pass to the task
		10, 	      //Priority
		NULL);	      //Task handle

    printf("main: Waiting for WiFi init\n");
    //wait untill init data is received
    while(!setup.wifi_conf.wifi_init){
        delay(1000);
    }
    printf("main: WiFi init ready\n");
    //Now we can init WiFi and start HTTP server
    initialise_wifi();
    printf("main: Starting HTTP server\n");
    xTaskCreate(&http_server,  //Function
                "http_server", //Name (for debug)
                2048,          //Stack deph
                &setup,        //Param's to pass to the task
                5,             //Priority
                NULL);         //Task handle
    
    return 0;
}

