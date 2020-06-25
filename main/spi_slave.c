/* SPI Slave example, receiver (uses SPI Slave driver to communicate with sender)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/igmp.h"

#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
//#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "soc/rtc_cntl_reg.h"
#include "esp32/rom/cache.h"
#include "driver/spi_slave.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "structures.h"
#include "driver/gpio.h"
//#include "rom/crc.h"
//#include "esp_heap_alloc_caps.h"

/*
SPI receiver (slave) example.

This example is supposed to work together with the SPI sender. It uses the standard SPI pins (MISO, MOSI, SCLK, CS) to 
transmit data over in a full-duplex fashion, that is, while the master puts data on the MOSI pin, the slave puts its own
data on the MISO pin.

This example uses one extra pin: GPIO_HANDSHAKE is used as a handshake pin. After a transmission has been set up and we're
ready to send/receive data, this code uses a callback to set the handshake pin high. The sender will detect this and start
sending a transaction. As soon as the transaction is done, the line gets set low again.
*/

/*
Pins in use. The SPI Master can use the GPIO mux, so feel free to change these if needed.
*/
//#define GPIO_HANDSHAKE 2
//#define GPIO_MOSI 12
//#define GPIO_MISO 13
//#define GPIO_SCLK 15
//#define GPIO_CS 14
#define FN_OK false
#define FN_ERR true

//Global variables
setup_type *Setup;
spi_bus_config_t buscfg;
spi_slave_interface_config_t slvcfg;
gpio_config_t io_conf;
char *TXbuf;
char *RXbuf;
spi_slave_transaction_t t;
uint16_t run_counter;


//Called after a transaction is queued and ready for pickup by master. We use this to set the handshake line high.
void my_post_setup_cb(spi_slave_transaction_t *trans) {
    WRITE_PERI_REG(GPIO_OUT_W1TS_REG, (1<<CONFIG_SPI_DC_PIN));
}

//Called after transaction is sent/received. We use this to set the handshake line low.
void my_post_trans_cb(spi_slave_transaction_t *trans) {
    WRITE_PERI_REG(GPIO_OUT_W1TC_REG, (1<<CONFIG_SPI_DC_PIN));
}

uint16_t crc16(char * arr, int n){
    uint16_t crc = 0;
    for (int c = 0; c < n; c++){
        crc ^= arr[c];
        for (int b = 0; b < 8; b++){
            if (crc & 1)
                crc = (crc >> 1) ^ 0x8408;
            else
                crc = (crc >> 1);
        }
    }
    return crc;
}

//Init SPI slave
bool SPI_init(){
    esp_err_t ret;

    //Configuration for the SPI bus
    buscfg.mosi_io_num=CONFIG_SPI_MOSI_PIN;
    buscfg.miso_io_num=CONFIG_SPI_MISO_PIN;
    buscfg.sclk_io_num=CONFIG_SPI_CLK_PIN;

    //Configuration for the SPI slave interface
    slvcfg.mode=CONFIG_SPI_MODE;
    slvcfg.spics_io_num=CONFIG_SPI_CS_PIN;
    slvcfg.queue_size=CONFIG_SPI_QUEUE_SIZE;
    slvcfg.flags=0;//SPI_DEVICE_HALFDUPLEX,
    slvcfg.post_setup_cb=my_post_setup_cb;
    slvcfg.post_trans_cb=my_post_trans_cb;

    //Configuration for the handshake line
    io_conf.intr_type=GPIO_INTR_DISABLE;
    io_conf.mode=GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask=(1<<CONFIG_SPI_DC_PIN);

    //Configure handshake line as output
    gpio_config(&io_conf);
    //Enable pull-ups on SPI lines so we don't detect rogue pulses when no master is connected.
    gpio_set_pull_mode(CONFIG_SPI_MOSI_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(CONFIG_SPI_MISO_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(CONFIG_SPI_CLK_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(CONFIG_SPI_CS_PIN, GPIO_PULLUP_ONLY);

    printf("AJ data ready starting INIT\n");
    //Initialize SPI slave interface
    ret=spi_slave_initialize(HSPI_HOST, &buscfg, &slvcfg, 1);
    if (ret!=ESP_OK){
        printf("SPI Error: INIT returned.\n%d", ret);
        return FN_ERR;
    }

    printf("AJ SPI initialized, preparing data.\n");
    memset(&t, 0, sizeof(t));
    t.tx_buffer=TXbuf;
    t.rx_buffer=RXbuf;
    memset(RXbuf, 8, 33);

    return FN_OK;
}

//Read AP data via SPI and store it to WiFiSetup via pointer
void readAP1(){
    //prepare CRC in reply
    uint16_t rec = RXbuf[47] + (RXbuf[48]<<8);
    uint16_t cal = crc16(RXbuf, 47);
    printf("CRC received=%d\n", rec);
    printf("CRC calculated=%d\n\n", cal);

    if (rec != cal){
        TXbuf[0] = 0;
        TXbuf[1] = 0;
        return;
    }

    //decode received bits
    Setup->wifi_conf.wifi_mode = (RXbuf[1] & (1<<0)) == 0;
    Setup->wifi_conf.ap_conf.ssid_hidden = (RXbuf[1]>>1) & 1; //T->0 (broadcast)/F->1 (hide)
    Setup->wifi_conf.ap_conf.authmode = (RXbuf[1]>>2) & 7; //3 bits
    Setup->wifi_conf.ap_conf.max_connection = ((RXbuf[1]>>5) & 3) + 1; //2 bits 1..4
    printf("AP state=\"%d\", %d\n", RXbuf[1], Setup->wifi_conf.wifi_mode);

    //receive Channel
    Setup->wifi_conf.ap_conf.channel = RXbuf[2];

    //receive SSID
    memcpy(Setup->wifi_conf.ap_conf.ssid, &RXbuf[3], 32);

    //receive IP
    memcpy(Setup->wifi_conf.ap_ip, &RXbuf[35], 4);

    //receive GW
    memcpy(Setup->wifi_conf.ap_gw, &RXbuf[39], 4);

    //receive Mask
    memcpy(Setup->wifi_conf.ap_mask, &RXbuf[43], 4);

    //Set default beacon interval
    Setup->wifi_conf.ap_conf.beacon_interval = 100;

    //bad data received. Reset to default.
    if (RXbuf[3] == 0xFF){
        printf("resetting to default AP\n");
        Setup->wifi_conf.wifi_mode = false; // AP mode
        Setup->wifi_conf.ap_conf.ssid_hidden = 0; // Broadcast SSID
        Setup->wifi_conf.ap_conf.authmode = 0;
        Setup->wifi_conf.ap_conf.max_connection = 4;
        Setup->wifi_conf.ap_conf.channel = 1;
        Setup->wifi_conf.ap_conf.ssid[0] = 'M';
        Setup->wifi_conf.ap_conf.ssid[1] = 'G';
        Setup->wifi_conf.ap_conf.ssid[2] = 'R';
        Setup->wifi_conf.ap_conf.ssid[3] = 0;
        Setup->wifi_conf.ap_ip[0] = 192;
        Setup->wifi_conf.ap_ip[1] = 168;
        Setup->wifi_conf.ap_ip[2] = 0;
        Setup->wifi_conf.ap_ip[3] = 1;
        Setup->wifi_conf.ap_gw[0] = 192;
        Setup->wifi_conf.ap_gw[1] = 168;
        Setup->wifi_conf.ap_gw[2] = 0;
        Setup->wifi_conf.ap_gw[3] = 1;
        Setup->wifi_conf.ap_mask[0] = 255;
        Setup->wifi_conf.ap_mask[1] = 255;
        Setup->wifi_conf.ap_mask[2] = 255;
        Setup->wifi_conf.ap_mask[3] = 0;
    }
    printf("AP channel=\"%d\"\n", Setup->wifi_conf.ap_conf.channel);
    printf("AP SSID=\"%s\"\n", Setup->wifi_conf.ap_conf.ssid);
    printf("AP IP=%d.%d.%d.%d\n", Setup->wifi_conf.ap_ip[0], 
                                  Setup->wifi_conf.ap_ip[1], 
                                  Setup->wifi_conf.ap_ip[2], 
                                  Setup->wifi_conf.ap_ip[3]);
    printf("AP GW=%d.%d.%d.%d\n", Setup->wifi_conf.ap_gw[0],
                                  Setup->wifi_conf.ap_gw[1], 
                                  Setup->wifi_conf.ap_gw[2], 
                                  Setup->wifi_conf.ap_gw[3]);
    printf("AP Mask=%d.%d.%d.%d\n", Setup->wifi_conf.ap_mask[0],
                                    Setup->wifi_conf.ap_mask[1],
                                    Setup->wifi_conf.ap_mask[2],
                                    Setup->wifi_conf.ap_mask[3]);


    //reply by CRC16
    TXbuf[0] = RXbuf[47];
    TXbuf[1] = RXbuf[48];
}

//Read AP data via SPI and store it to WiFiSetup via pointer
void readAP2(){
    //prepare CRC in reply
    uint16_t rec = RXbuf[65] + (RXbuf[66]<<8);
    uint16_t cal = crc16(RXbuf, 65);
    printf("CRC received=%d\n", rec);
    printf("CRC calculated=%d\n\n", cal);

    if (rec != cal){
        TXbuf[0] = 0;
        TXbuf[1] = 0;
        return;
    }

    if (RXbuf[1] == 0xFF){
        //receive PW
        Setup->wifi_conf.ap_conf.password[0] = '1';
        Setup->wifi_conf.ap_conf.password[1] = '2';
        Setup->wifi_conf.ap_conf.password[2] = '3';
        Setup->wifi_conf.ap_conf.password[3] = '4';
        Setup->wifi_conf.ap_conf.password[4] = '5';
        Setup->wifi_conf.ap_conf.password[5] = '6';
        Setup->wifi_conf.ap_conf.password[6] = '7';
        Setup->wifi_conf.ap_conf.password[7] = '8';
        Setup->wifi_conf.ap_conf.password[8] = 0;
    }
    else
        memcpy(Setup->wifi_conf.ap_conf.password, &RXbuf[1], 64);

    //reply by CRC16
    TXbuf[0] = RXbuf[65];
    TXbuf[1] = RXbuf[66];
}

//Read STA1 data via SPI and store it to WiFiSetup via pointer
void readSTA1(){
    //prepare CRC in reply
    uint16_t rec = RXbuf[40] + (RXbuf[41]<<8);
    uint16_t cal = crc16(RXbuf, 40);
    printf("\nCRC received=%d\n", rec);
    printf("CRC calculated=%d\n\n", cal);

    if (rec != cal){
        TXbuf[0] = 0;
        TXbuf[1] = 0;
        return;
    }

    //decode received bits
    Setup->wifi_conf.wifi_mode = (RXbuf[1] & (1<<0)) == 1;
    Setup->wifi_conf.sta_conf.bssid_set = (RXbuf[1]>>1) & 1;
    printf("STA state=\"%d\", %d\n", RXbuf[1], Setup->wifi_conf.wifi_mode);

    //receive SSID
    memcpy(Setup->wifi_conf.sta_conf.ssid, &RXbuf[2], 32);

    //receive BSSID
    memcpy(Setup->wifi_conf.sta_conf.bssid, &RXbuf[34], 6);

    if (RXbuf[2] == 0xFF){
        Setup->wifi_conf.wifi_mode = false;
        Setup->wifi_conf.sta_conf.bssid_set = false;
        Setup->wifi_conf.sta_conf.ssid[0] = 0;
        Setup->wifi_conf.sta_conf.bssid[0] = 0;
    }

    printf("STA SSID=\"%s\"\n", Setup->wifi_conf.sta_conf.ssid);

    //reply by CRC16
    TXbuf[0] = RXbuf[40];
    TXbuf[1] = RXbuf[41];    
}

//Read STA2 data via SPI and store it to WiFiSetup via pointer
void readSTA2(){
    //prepare CRC in reply
    uint16_t rec = RXbuf[65] + (RXbuf[66]<<8);
    uint16_t cal = crc16(RXbuf, 65);
    printf("CRC received=%d\n", rec);
    printf("CRC calculated=%d\n\n", cal);

    if (rec != cal){
        TXbuf[0] = 0;
        TXbuf[1] = 0;
        return;
    }

    //receive PW
    memcpy(Setup->wifi_conf.sta_conf.password, &RXbuf[1], 64);

    //reply by CRC16
    TXbuf[0] = RXbuf[65];
    TXbuf[1] = RXbuf[66];
    
    //set initialized state
    Setup->wifi_conf.wifi_init = true;
    printf("SPI: Wifi setup readed. Init Enabled\n");
}

//Read Mode
void readMode(){
}

//Read Mode
void readModeID(){
}

//Read new measurement of driver
void readMeas(int8_t *timerStep){
    //prepare CRC in reply
    uint16_t rec = RXbuf[5] + (RXbuf[6]<<8);
    uint16_t cal = crc16(RXbuf, 5);
    printf("CRC received=%d\n", rec);
    printf("CRC calculated=%d\n\n", cal);

    if (rec != cal){
        TXbuf[0] = 0;
        TXbuf[1] = 0;
        return;
    }

    //reply by CRC16
    TXbuf[0] = RXbuf[5];
    TXbuf[1] = RXbuf[6];

    if (readings.len==0){
        return;
    }

    for (int i = readings.len-1; i>0; i--){
        readings.list[i].id = readings.list[i-1].id;
        readings.list[i].value = readings.list[i-1].value;
        readings.list[i].referee = readings.list[i-1].referee;
    }

    readings.list[0].id = run_counter;
    run_counter ++;

    readings.list[0].value = RXbuf[1] + (RXbuf[2]<<8) + (RXbuf[3]<<16) + (RXbuf[4]<<24);
    readings.list[0].referee = 0;
    *timerStep = 3;
    printf("readMeas: %d - %d", readings.list[0].id, readings.list[0].value);
}

void readSCMD(int8_t *timerStep){
    //prepare CRC in reply
    uint16_t rec = RXbuf[2] + (RXbuf[3]<<8);
    uint16_t cal = crc16(RXbuf, 2);
    printf("CRC received=%d\n", rec);
    printf("CRC calculated=%d\n\n", cal);

    if (rec != cal){
        TXbuf[63] = 0;
        TXbuf[64] = 0;
        return;
    }

    *timerStep = (int8_t)RXbuf[1];
    
    //reply by CRC16
    TXbuf[63] = RXbuf[2];
    TXbuf[64] = RXbuf[3];

    uint16_t retCRC = 0;
    TXbuf[0] = 0; //ESP has nothing to do.

    if(Setup->wifi_conf.wifi_init != true){
        printf("SPI SCMD: requesting re-init\n");
        TXbuf[0] = 1; //ESP has been reset.
    }
    else if(Setup->wifi_conf.save == 1){
        printf("SPI SCMD: requesting SaveData AP1\n");
        TXbuf[0] = 2; //User has pressed "save" AP1.

       //decode received bits
       TXbuf[1] = (Setup->wifi_conf.wifi_mode == false) & 1;
       TXbuf[1] |= Setup->wifi_conf.ap_conf.ssid_hidden<<1; //T->0 (broadcast)/F->1 (hide)
       TXbuf[1] |= (Setup->wifi_conf.ap_conf.authmode & 7)<<2;//3 bits
       TXbuf[1] |= ((Setup->wifi_conf.ap_conf.max_connection-1) & 3)<<5; //2 bits 1..4

       printf("mode=%d, ssid hide=%d, auth mode=%d, max con=%d\n", 
              Setup->wifi_conf.wifi_mode, Setup->wifi_conf.ap_conf.ssid_hidden,
              Setup->wifi_conf.ap_conf.authmode, Setup->wifi_conf.ap_conf.max_connection);

       //receive Channel
       TXbuf[2] = Setup->wifi_conf.ap_conf.channel;

       //receive SSID
       memcpy(&TXbuf[3], Setup->wifi_conf.ap_conf.ssid, 32);

       //receive IP
       memcpy(&TXbuf[35], Setup->wifi_conf.ap_ip, 4);

       //receive GW
       memcpy(&TXbuf[39], Setup->wifi_conf.ap_gw, 4);

       //receive Mask
       memcpy(&TXbuf[43], Setup->wifi_conf.ap_mask, 4);
    }
    else if(Setup->wifi_conf.save == 2){
        printf("SPI SCMD: requesting SaveDataAP2\n");
        TXbuf[0] = 3; //User has pressed "save" AP1.

        //receive PW
        memcpy(&TXbuf[1], Setup->wifi_conf.ap_conf.password, 64);
    }
    else if(Setup->wifi_conf.save == 3){
        printf("SPI SCMD: requesting SaveDataSTA1\n");
        TXbuf[0] = 4; //User has pressed "save" AP1.

        //decode received bits
        TXbuf[1] = Setup->wifi_conf.wifi_mode & 1;
        TXbuf[1] |= (Setup->wifi_conf.sta_conf.bssid_set & 1) << 1;

        //receive SSID
        memcpy(&TXbuf[2], Setup->wifi_conf.sta_conf.ssid, 32);

        //receive BSSID
        memcpy(&TXbuf[34], Setup->wifi_conf.sta_conf.bssid, 6);
    }
    else if(Setup->wifi_conf.save == 4){
        printf("SPI SCMD: requesting SaveDataSTA2\n");
        TXbuf[0] = 5; //User has pressed "save" AP1.

        //receive PW
        memcpy(&TXbuf[1], Setup->wifi_conf.sta_conf.password, 64);
    }
    else if(Setup->wifi_conf.save == 6){
        printf("SPI SCMD: sending User Active\n");
        TXbuf[0] = 6; //User Active
    }
    else if(Setup->wifi_conf.save == 7){
        printf("SPI SCMD: sending connected to WiFi/AP started\n");
        TXbuf[0] = 7; //User Active
    }

    retCRC = crc16(TXbuf, 63);
    TXbuf[65] = (uint8_t)retCRC;
    TXbuf[66] = (uint8_t)(retCRC>>8);

    printf("SPI: SCMD sending:");
    for (int i=0; i<67; i++){
        printf(" %X", TXbuf[i]);
    }
    printf("\n");
}

void readDEBUG(){
    //prepare CRC in reply
    uint16_t rec = RXbuf[65] + (RXbuf[66]<<8);
    uint16_t cal = crc16(RXbuf, 65);
    printf("CRC received=%d\n", rec);
    printf("CRC calculated=%d\n\n", cal);

    if (rec != cal){
        TXbuf[0] = 0;
        TXbuf[1] = 0;
        printf("SPI warning: CRC error\n");
    }

    TXbuf[0] = RXbuf[65];
    TXbuf[1] = RXbuf[66];
    
    printf("SPI: DEBUG received:");
    for (int i=0; i<67; i++){
        printf(" %X", RXbuf[i]);
    }
    printf("\n");
}

//Main application
void spi_slave(void *parSetup)
{
    esp_err_t ret;
    Setup = parSetup;
    run_counter = 0;
    RXbuf = malloc(67);
    if (RXbuf == NULL){
      printf("Error: RXbuf malloc failed.\n");
      return;
    }
    TXbuf = malloc(67);
    if (TXbuf == NULL){
      printf("Error: TXbuf malloc failed.\n");
      free(RXbuf);
      return;
    }

    printf("AJ starting init\n");
    if (SPI_init() == FN_ERR)
        return;

    while(1) {
        //Set up a transaction of data
        t.length=8*67;

        /* This call enables the SPI slave interface to send/receive to the sendbuf and recvbuf. The transaction is
        initialized by the SPI master, however, so it will not actually happen until the master starts a hardware transaction
        by pulling CS low and pulsing the clock etc. In this specific example, we use the handshake line, pulled up by the
        .post_setup_cb callback that is called as soon as a transaction is ready, to let the master know it is free to transfer
        data.
        */

        RXbuf[0] = 0;
        ret=spi_slave_transmit(HSPI_HOST, &t, portMAX_DELAY);
        assert(ret==ESP_OK);
        printf("SPI received:\n");
        printf("SPI wifi->Save = %d\n", Setup->wifi_conf.save);
        
        switch(RXbuf[0]){
            case 1:
                printf("AJ reading AP1 config\n");
                readAP1();
                break;
            case 2:
                printf("AJ reading AP2 config\n");
                readAP2();
                break;
            case 3:
                printf("AJ reading STA1 config\n");
                readSTA1();
                break;
            case 4:
                printf("AJ reading STA2 config\n");
                readSTA2();
                break;
            case 5:
                printf("AJ reading Modes config\n");
                readMode();
                break;
            case 6:
                printf("AJ reading Mode ID\n");
                readModeID();
                break;
            case 7:
                printf("AJ reading Meas value\n");
                readMeas(&readings.timerStep);
                break;
            case 128:
                printf("AJ SCMD request received\n");
                readSCMD(&readings.timerStep);

                if (readings.len == 0){
                    readings.len = 500;
                    readings.list = malloc(sizeof(reading_type[readings.len]));
                    if (readings.list == NULL){
                      printf("Error: readings.list malloc failed.\n");
                      break;
                    }
                    //generate readings.list
                    for (int i = 0; i<readings.len; i++){
                        readings.list[i].id = 0;
                        readings.list[i].value = 0;
                        readings.list[i].referee = 0;
                    }
                }

                break;
            case 129:
                printf("AJ SCMD AP1 ack received\n");
                Setup->wifi_conf.save = 2;
                break;
            case 130:
                printf("AJ SCMD AP2 ack received\n");
                Setup->wifi_conf.save = 3;
                break;
            case 131:
                printf("AJ SCMD STA1 ack received\n");
                Setup->wifi_conf.save = 4;
                break;
            case 132:
                printf("AJ SCMD STA2 ack received\n");
                Setup->wifi_conf.save = 0; 
                esp_restart();
                break;
            case 133:
                printf("AJ SCMD user active ack received\n");
                Setup->wifi_conf.save = 0;
                break;
            case 134:
                printf("AJ SCMD WiFi Done ack received\n");
                Setup->wifi_conf.save = 0;
                break;
            case 255:
                printf("AJ DEBUG request received\n");
                readDEBUG();
                break;
            default:
                TXbuf[0] = 0;
                TXbuf[1] = 0;
                break;
        }
    }
    printf("SPI: freeing RX/TX buffers\n");
    free(TXbuf);
    free(RXbuf);
}
