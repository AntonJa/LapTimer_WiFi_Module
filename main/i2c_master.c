/* SPI Slave example, receiver (uses SPI Slave driver to communicate with sender)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
//#include <stdint.h>
//#include <stddef.h>
//#include <string.h>

/* #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "freertos/semphr.h"
 #include "freertos/queue.h"

 #include "lwip/sockets.h"
 #include "lwip/dns.h"
 #include "lwip/netdb.h"
 #include "lwip/igmp.h"*/

// #include "esp_wifi.h"
// #include "esp_wifi_types.h"
/* #include "esp_system.h"
 #include "esp_event.h"
 #include "esp_event_loop.h"
 #include "nvs_flash.h"
 #include "soc/rtc_cntl_reg.h"
 #include "esp32/rom/cache.h"*/
#include "driver/i2c.h"
#include "esp_console.h"
#include "esp_log.h"
// #include "esp_spi_flash.h"
 #include "structures.h"
// #include "driver/gpio.h"
//#include "rom/crc.h"
//#include "esp_heap_alloc_caps.h"

#define delay(ms) (vTaskDelay(ms/portTICK_RATE_MS))
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define WRITE_BIT I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ    /*!< I2C master read */
#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                 /*!< I2C ack value */
#define NACK_VAL 0x1                /*!< I2C nack value */

#define PART_STR "I2C: "
#define LOG(str, ... ) printf(PART_STR);\
                       printf(str, ##__VA_ARGS__)

static gpio_num_t i2c_gpio_sda = 14;//18;//13;
static gpio_num_t i2c_gpio_scl = 13;//19;//9;
static uint32_t i2c_frequency = 100000;
static i2c_port_t i2c_port = I2C_NUM_0;
ds3231_conf_type* ds_conf;

static esp_err_t i2c_master_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = i2c_gpio_sda,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = i2c_gpio_scl,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = i2c_frequency
    };
    i2c_param_config(i2c_port, &conf);
    return i2c_driver_install(i2c_port, 
                              I2C_MODE_MASTER, 
                              I2C_MASTER_RX_BUF_DISABLE, 
                              I2C_MASTER_TX_BUF_DISABLE, 0);
}

static int do_i2c_read(){
  uint8_t data;

  LOG("master read\n");
  for (int i = 0; i<19; i++){
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    data = 0;
      
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x68 << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, i, ACK_CHECK_EN);
      
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x68 << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &data, NACK_VAL);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
      //printf("%02x ", data);
      switch(i){
        case 0:
          ds_conf->bcd_seconds = data;
          break;
        case 1:
          ds_conf->bcd_minutes = data;
          break;
        case 2:
          ds_conf->bcd_hour = data;
          break;
        case 3:
          ds_conf->bcd_week_day = data;
          break;
        case 4:
          ds_conf->bcd_date = data;
          break;
        case 5:
          ds_conf->bcd_month = data & 0x7F;

          if(data & 0x80)
            ds_conf->century = true;
          else
            ds_conf->century = false;
          break;
        case 6:
          ds_conf->bcd_year = data;
          break;
        case 14:
          if(data & 0x01)
            ds_conf->A1IE = true;
          else
            ds_conf->A1IE = false;

          if(data & 0x02)
            ds_conf->A2IE = true;
          else
            ds_conf->A2IE = false;

          if(data & 0x04)
            ds_conf->INTCN = true;
          else
            ds_conf->INTCN = false;

          if(data & 0x08)
            ds_conf->RS1 = true;
          else
            ds_conf->RS1 = false;

          if(data & 0x10)
            ds_conf->RS2 = true;
          else
            ds_conf->RS2 = false;

          if(data & 0x20)
            ds_conf->CONV = true;
          else
            ds_conf->CONV = false;

          if(data & 0x40)
            ds_conf->BBSQW = true;
          else
            ds_conf->BBSQW = false;

          if(data & 0x80)
            ds_conf->EOSC = true;
          else
            ds_conf->EOSC = false;
          break;
        case 15:
          if(data & 0x01)
            ds_conf->A1F = true;
          else
            ds_conf->A1F = false;

          if(data & 0x02)
            ds_conf->A2F = true;
          else
            ds_conf->A2F = false;

          if(data & 0x04)
            ds_conf->BSY = true;
          else
            ds_conf->BSY = false;

          if(data & 0x08)
            ds_conf->EN32kHz = true;
          else
            ds_conf->EN32kHz = false;

          if(data & 0x80)
            ds_conf->OSF = true;
          else
            ds_conf->OSF = false;
          break;
        case 16:
          ds_conf->aging = data;
          break;
        case 17:
          if(data & 0x80){
            ds_conf->temperature = 0xFC00;
          }
          else{
            ds_conf->temperature = 0x0;
          }
          ds_conf->temperature += (uint16_t)data<<2;
          break;
        case 18:
          ds_conf->temperature += (uint16_t)data>>6;
          break;
        default:
          break;
      }
    } else {
      LOG("Err\n");
    }
  }

  printf("\n");
  return 0;
}

static int do_i2c_write(){
  uint8_t data;

  LOG("master write\n");
  for (int i = 0; i<19; i++){
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    data = 0;
    
    switch(i){
      case 0:
        data = ds_conf->bcd_seconds;
        break;
      case 1:
        data = ds_conf->bcd_minutes;
        break;
      case 2:
        data = ds_conf->bcd_hour;
        break;
      case 3:
        data = ds_conf->bcd_week_day;
        break;
      case 4:
        data = ds_conf->bcd_date;
        break;
      case 5:
        data = ds_conf->bcd_month;

        if(ds_conf->century)
          data |= 0x80;
        else
          data &= 0x7F;
        break;
      case 6:
        data = ds_conf->bcd_year;
        break;
      case 14:
        if(ds_conf->A1IE)
          data |= 1;
        else
          data &= ~(1);

        if(ds_conf->A2IE)
          data |= 1<<1;
        else
          data &= ~(1<<1);

        if(ds_conf->INTCN)
          data |= 1<<2;
        else
          data &= ~(1<<2);

        if(ds_conf->RS1)
          data |= 1<<3;
        else
          data &= ~(1<<3);

        if(ds_conf->RS2)
          data |= 1<<4;
        else
          data &= ~(1<<4);

        if(ds_conf->CONV)
          data |= 1<<5;
        else
          data &= ~(1<<5);

        if(ds_conf->BBSQW)
          data |= 1<<6;
        else
          data &= ~(1<<6);

        if(ds_conf->EOSC)
          data |= 1<<7;
        else
          data &= ~(1<<7);
        break;
      case 15:
        if(ds_conf->EN32kHz)
          data |= 1<<3;
        else
          data &= ~(1<<3);

        if(ds_conf->OSF)
          data |= 1<<7;
        else
          data &= ~(1<<7);
        break;
      case 16:
        data = ds_conf->aging;
        break;
      case 17:
        data = (uint8_t)ds_conf->temperature>>2;
        break;
      case 18:
        data = (uint8_t)ds_conf->temperature<<6;
        break;
      default:
        continue;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x68 << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, i, ACK_CHECK_EN);
      
    i2c_master_write_byte(cmd, data, NACK_VAL);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
      printf("%02x ", data);
    } else {
      LOG("Err\n");
    }
  }

  printf("\n");
  return 0;
}
uint8_t extract_bcd(uint8_t value, char* str){
  uint8_t id = 0;
  if(((value & 0xf) > 9) || (value>>4 > 9))
    return id;
  if(value > 9){
    str[id] = (value>>4) + '0';
    id++;
  }
  str[id] = (value & 0xf) + '0';
  id++;
  str[id] = 0;
  return id;
}

void generate_date_time(){
  /*uint8_t id = 0;
  id = extract_bcd(ds_conf->bcd_date, (char*)&ds_conf->date_time_ro);
  ds_conf->date_time_ro[id] = '-';
  id++;
  id += extract_bcd(ds_conf->bcd_month, (char*)&ds_conf->date_time_ro[id]);
  ds_conf->date_time_ro[id] = '-';
  id++;
  ds_conf->date_time_ro[id] = '2';
  id++;
  if(ds_conf->century){
    ds_conf->date_time_ro[id] = '1';
  }else{
    ds_conf->date_time_ro[id] = '0';
  }
  id++;
  id += extract_bcd(ds_conf->bcd_year, (char*)&ds_conf->date_time_ro[id]);
  ds_conf->date_time_ro[id] = ' ';
  id++;
  id += extract_bcd(ds_conf->bcd_hour, (char*)&ds_conf->date_time_ro[id]);
  ds_conf->date_time_ro[id] = ':';
  id++;
  id += extract_bcd(ds_conf->bcd_minutes, (char*)&ds_conf->date_time_ro[id]);
  ds_conf->date_time_ro[id] = ':';
  id++;
  id += extract_bcd(ds_conf->bcd_seconds, (char*)&ds_conf->date_time_ro[id]);*/
  if(ds_conf->century){
    sprintf(ds_conf->date_time_ro, 
            "%02x-%02x-21%02x %02x:%02x:%02x",
            ds_conf->bcd_date,
            ds_conf->bcd_month,
            ds_conf->bcd_year,
            ds_conf->bcd_hour,
            ds_conf->bcd_minutes,
            ds_conf->bcd_seconds);
  }else{
    sprintf(ds_conf->date_time_ro, 
            "%02x-%02x-20%02x %02x:%02x:%02x",
            ds_conf->bcd_date,
            ds_conf->bcd_month,
            ds_conf->bcd_year,
            ds_conf->bcd_hour,
            ds_conf->bcd_minutes,
            ds_conf->bcd_seconds);
  }
  LOG("%s\n", ds_conf->date_time_ro);
}

//Main application
void i2c_master(void *parSetup){
  ds_conf = parSetup;
  ds_conf->i2c_waiting = false;
  ds_conf->i2c_busy = false;
  ds_conf->www_busy = false;
  LOG("master start\n");
  ESP_ERROR_CHECK(i2c_master_init());
  while(1){
    LOG("DS: decidion start\n");
    ds_conf->i2c_busy = true;
    while(ds_conf->www_busy){
      ds_conf->i2c_waiting = true;
    }
    if(ds_conf->changed){
      do_i2c_write();
      ds_conf->changed = false;
    }else{
      do_i2c_read();
      generate_date_time();
    }
    ds_conf->i2c_busy = false;
    delay(800);
  }
  i2c_driver_delete(i2c_port);
}
