#ifndef __FN_DEFS_H__
#define __FN_DEFS_H__

#include "esp_system.h"

void append_id(char *result, uint16_t value);
void append_val(char *result, uint32_t value);
void decode_ip(uint8_t* ip, char* buf);
void decode_mac(uint8_t* ip, char* buf);
int s2i(char* buf);
int s2bcd(char* buf);
void http_server(void *pvParameters);
void spi_slave(void *pvParameters);
void i2c_master(void *);
void nvs_rw_init(void *);
esp_err_t nvs_set_wifi_conf();
esp_err_t spi_init();
esp_err_t nvs_reset_wifi_conf();

#endif //__FN_DEFS_H__
