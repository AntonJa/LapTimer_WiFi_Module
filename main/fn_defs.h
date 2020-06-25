#ifndef __FN_DEFS_H__
#define __FN_DEFS_H__

#include "esp_system.h"

void http_server(void *pvParameters);
void spi_slave(void *pvParameters);
void i2c_master(void *);
void nvs_rw_init();
esp_err_t spi_init();

#endif //__FN_DEFS_H__
