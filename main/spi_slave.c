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
#include "fn_defs.h"
//#include "rom/crc.h"
//#include "esp_heap_alloc_caps.h"

#define PART_STR "SPI (%d): "
#define LOG(str, ... ) printf(PART_STR, xTaskGetTickCount());\
                       printf(str, ##__VA_ARGS__)

#define READ_NONE 0
#define READ_INIT 1
#define READ_MEASURE 2

#define WRITE_NONE 0
#define WRITE_INIT 1
#define WRITE_MODES 2

#define BIT_USER_ACTIVE 0
#define BIT_WIFI_CONNECTED 1

typedef struct{
   uint16_t sentCRC[3];
   uint8_t sentSCMD[2];
} exchange_type;

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
spi_slave_transaction_t transaction;
uint16_t run_counter;
int showed_bikes;
int prevModeType;
int prevMode;
bool modeTypeChanged;

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

    LOG("data ready starting INIT\n");
    //Initialize SPI slave interface
    ret=spi_slave_initialize(HSPI_HOST, &buscfg, &slvcfg, 1);
    if (ret!=ESP_OK){
        LOG("Error: INIT returned.\n%d", ret);
        return FN_ERR;
    }

    LOG("SPI initialized, preparing data.\n");
    memset(&transaction, 0, sizeof(transaction));
    transaction.tx_buffer=TXbuf;
    transaction.rx_buffer=RXbuf;
    memset(RXbuf, 8, 33);

    return FN_OK;
}

//Read AP data via SPI and store it to WiFiSetup via pointer
void readInit(){
    Setup->modes.mode[0].lap_gym = (RXbuf[1] & (1<<0)) != 0;
    Setup->modes.mode[1].lap_gym = (RXbuf[1] & (1<<1)) != 0;
    Setup->modes.mode[2].lap_gym = (RXbuf[1] & (1<<2)) != 0;

    for(int i = 0; i<3; i++){
        Setup->modes.mode[i].bikes = RXbuf[i+2];
        if(Setup->modes.mode[i].bikes < 1)
            Setup->modes.mode[i].bikes = 1;
        if(Setup->modes.mode[i].bikes > 10)
            Setup->modes.mode[i].bikes = 10;
    }

    if((RXbuf[1] & (1<<7)) != 0){ // reset config
        nvs_reset_wifi_conf();
	LOG("reset config\n");
    }
}

void shiftDn(int add){
    int next = run_counter + add;
    if(next > readings.len)
        next = readings.len;
    for (int i = next-1; i>=add; i--){
        readings.list[i].id = readings.list[i-add].id;
        readings.list[i].lapGym = readings.list[i-add].lapGym;
        readings.list[i].bykeNr = readings.list[i-add].bykeNr;
	readings.list[i].bykes = readings.list[i-add].bykes;
        readings.list[i].value = readings.list[i-add].value;
        readings.list[i].referee = readings.list[i-add].referee;
    }
    for (int i = add-1; i>=0; i--){
    	readings.list[i].id = readings.list[i + 1].id + 1;
    }
    run_counter += add;
}

void shiftUp(){
    for (int i = 0; i<readings.len-1; i++){
        readings.list[i].id = readings.list[i+1].id;
        readings.list[i].lapGym = readings.list[i+1].lapGym;
        readings.list[i].bykeNr = readings.list[i+1].bykeNr;
	readings.list[i].bykes = readings.list[i+1].bykes;
        readings.list[i].value = readings.list[i+1].value;
        readings.list[i].referee = readings.list[i+1].referee;
    }
    run_counter--;
}

//Read new measurement of driver
void readMeas(int8_t *timerStep){
    if (readings.len==0)
        return;

    // RXbuf[0] - MCMDsetNewTime
    // RXbuf[1] - Mode Nr
    // RXbuf[2] - mode type Lap(0)/Gym(1)
    // RXbuf[3] - measured (Lap mode)/Bikes count (Gym mode)
    // RXbuf[4] - Step Nr (0..9 start running/ 10..19 stopping)
    // RXbuf[5..8]   - measurement 1
    // RXbuf[..]     - other measurements
    // RXbuf[41..44] - measurement 10

    if((RXbuf[1] != prevMode) && ((*timerStep != 0) || ((prevModeType == 0) && !modeTypeChanged))){
        shiftUp();
    }

    LOG("Step=%d, Prev=%d, sh_bik=%d", RXbuf[4], *timerStep, showed_bikes);
    if((RXbuf[4] < *timerStep) && (RXbuf[1] == prevMode)){ // Last bike finished. start again
        for(int i=0; i<(showed_bikes - (*timerStep - 10)); i++){
            int adr = (showed_bikes - i - 1)*4;
            readings.list[i].value = RXbuf[5+adr] + (RXbuf[6+adr]<<8) +
                                     (RXbuf[7+adr]<<16) + (RXbuf[8+adr]<<24);
        }
        showed_bikes = 0;
    }

    if(RXbuf[2] == 0){
        if(RXbuf[3] == 1){ // new measurement has arrived
            shiftDn(1);
            readings.list[1].lapGym = 0;
            readings.list[1].bykeNr = 1;
	    readings.list[1].bykes = 1;
            readings.list[1].value = RXbuf[9] + (RXbuf[10]<<8) +
                                     (RXbuf[11]<<16) + (RXbuf[12]<<24);
	    readings.list[0].referee = 0; // new entry must have default referee value
        }

        if((RXbuf[5] + (RXbuf[6]<<8) + (RXbuf[7]<<16) + (RXbuf[8]<<24)) > 0){ // wait first meas.
            if(modeTypeChanged){
                modeTypeChanged = false;
                shiftDn(1);
                readings.list[0].referee = 0;
            }
            readings.list[0].lapGym = 0;
            readings.list[0].bykeNr = 1;
	    readings.list[0].bykes = 1;
            readings.list[0].value = RXbuf[5] + (RXbuf[6]<<8) +
                                     (RXbuf[7]<<16) + (RXbuf[8]<<24);
        }
    }else{
        modeTypeChanged = true;
        // if mode is stopping, stopping bikes can be read from max bikes for current mode
        if(RXbuf[4] < 10){//step is not in stopping part.
            // if STEP = last_bike_nr + N, then N readings must be added
            // if STEP < last_bike_nr, then new run sequence is here and
            //   we have to add STEP count of readings (theoteticaly, STEP will be increasing by 1)
            int step = RXbuf[4];

            if( showed_bikes <= step ){
                // shift down measured values
                int add = step - showed_bikes;
                shiftDn(add);

                // add new measurements and update old
                for(int i=step-1; i>=0; i--){
                    readings.list[i].lapGym = 1;
                    readings.list[i].bykeNr = step - i;
		    readings.list[i].bykes = RXbuf[3];
                    int adr = (step - 1 - i) * 4;
                    readings.list[i].value = RXbuf[5+adr] + (RXbuf[6+adr]<<8) +
                                             (RXbuf[7+adr]<<16) + (RXbuf[8+adr]<<24);
                    // reset referee params only for new entries
                    if(i < add)
                        readings.list[i].referee = 0;
                }
                showed_bikes = step;
            }else{ // if(step < showed_bikes)
                // new lap started
                shiftDn(step);
                for(int i=0; i<step; i++){
                    readings.list[i].lapGym = 1;
                    readings.list[i].bykeNr = i;
		    readings.list[i].bykes = RXbuf[3];
                    int adr = (step - 1 - i) * 4;
                    readings.list[i].value = RXbuf[5+adr] + (RXbuf[6+adr]<<8) +
                                             (RXbuf[7+adr]<<16) + (RXbuf[8+adr]<<24);
                    readings.list[i].referee = 0;
                }
                showed_bikes = step;
            }
        }else{ // stopping part. Simply update all runs
            int bikes = RXbuf[3];
            if(showed_bikes < bikes){
                shiftDn(bikes - showed_bikes);
                for(int i=0; i<(bikes - showed_bikes); i++){
                    readings.list[i].lapGym = 1;
                    readings.list[i].bykeNr = bikes - i;
		    readings.list[i].bykes = RXbuf[3];
                    readings.list[i].referee = 0;
                }
	        showed_bikes = bikes;
            }
	    for(int i=0; i<bikes; i++){
                readings.list[bikes - i - 1].value = RXbuf[5+i*4] + (RXbuf[6+i*4]<<8) +
                                                     (RXbuf[7+i*4]<<16) + (RXbuf[8+i*4]<<24);
            }
        }
    }
    prevModeType = RXbuf[2];
    prevMode = RXbuf[1];
    *timerStep = RXbuf[4];
    LOG("val[0]=%d, val[1]=%d, val[2]=%d, val[3]=%d\n", readings.list[0].value, 
                                                        readings.list[1].value, 
                                                        readings.list[2].value, 
                                                        readings.list[3].value);
}

void writeNone(){
    //nothing much to do...
    TXbuf[0] = WRITE_NONE;
}

void writeInit(){
    TXbuf[0] = WRITE_INIT;
    //TXbuf[1] is status byte and it is define in this process main loop
    TXbuf[2] = Setup->wifi_conf.wifi_mode;
    // send actual SSID
    if(!Setup->wifi_conf.wifi_mode)
        memcpy(&TXbuf[3], Setup->wifi_conf.ap_conf.ssid, 32);
    else
        memcpy(&TXbuf[3], Setup->wifi_conf.sta_conf.ssid, 32);
}

void writeModes(){
    TXbuf[0] = WRITE_MODES;
    //TXbuf[1] is status byte and it is define in this process main loop
    TXbuf[2] = 0;
    TXbuf[4] = 0;
    TXbuf[6] = 0;
    if(Setup->modes.mode[0].lap_gym){
        TXbuf[2] = 1;
    }
    TXbuf[5] = Setup->modes.mode[0].bikes;

    if(Setup->modes.mode[1].lap_gym){
        TXbuf[3] = 1;
    }
    TXbuf[6] = Setup->modes.mode[1].bikes;

    if(Setup->modes.mode[2].lap_gym){
        TXbuf[4] = 1;
    }
    TXbuf[7] = Setup->modes.mode[2].bikes;
}

//Main application
void spi_slave(void *parSetup)
{
    esp_err_t ret;
    Setup = parSetup;
    run_counter = 0;
    showed_bikes = 0;
    prevModeType = -1;
    prevMode = 0;
    uint8_t repeatSCMD;
    exchange_type exchg;
    bool initSent = false;
    modeTypeChanged = false;

    // init "exchg"
    for(int i=0; i<3; i++){
        exchg.sentCRC[i] = 0;
        exchg.sentSCMD[i] = 0;
    }

    RXbuf = malloc(67);
    if (RXbuf == NULL){
      LOG("Error: RXbuf malloc failed.\n");
      return;
    }
    TXbuf = malloc(67);
    if (TXbuf == NULL){
      LOG("Error: TXbuf malloc failed.\n");
      free(RXbuf);
      return;
    }

    LOG("starting init\n");
    if (SPI_init() == FN_ERR)
        return;

    if (readings.len == 0){
        readings.len = 500;
        readings.list = malloc(sizeof(reading_type[readings.len]));
        if (readings.list == NULL){
            LOG("Error: readings.list malloc failed.\n");
        }
        //generate readings.list
        for (int i = 0; i<readings.len; i++){
            readings.list[i].id = 0;
	    readings.list[i].lapGym = 0;
	    readings.list[i].bykeNr = 0;
	    readings.list[i].bykes = 0;
            readings.list[i].value = 0;
            readings.list[i].referee = 0;
        }
    }

    while(1) {
        //Set up a transaction of data
        transaction.length=8*67;

        /* This call enables the SPI slave interface to send/receive to the sendbuf and recvbuf. The transaction is
        initialized by the SPI master, however, so it will not actually happen until the master starts a hardware transaction
        by pulling CS low and pulsing the clock etc. In this specific example, we use the handshake line, pulled up by the
        .post_setup_cb callback that is called as soon as a transaction is ready, to let the master know it is free to transfer
        data.
        */

        RXbuf[0] = 0;

	// show SENT values
	for (int i=0; i<67; i++){
            printf(" %X", TXbuf[i]);
        }
        printf("\n");

        // exchange data
	ret=spi_slave_transmit(HSPI_HOST, &transaction, portMAX_DELAY);
        
	// show RECEIVED values
	for (int i=0; i<67; i++){
            printf(" %X", RXbuf[i]);
        }
        printf("\n");

        assert(ret==ESP_OK);
        LOG("received: %d\n", RXbuf[0]);

        //prepare CRC in reply
        uint16_t rec = RXbuf[65] + (RXbuf[66]<<8);
        uint16_t cal = crc16(RXbuf, 65);
        LOG("CRC received=%d\n", rec);
        LOG("CRC calculated=%d\n\n", cal);
        TXbuf[63] = (uint8_t)cal;
        TXbuf[64] = (uint8_t)(cal>>8);

	// If received data is consistent, process it
	if(rec == cal){
            switch(RXbuf[0]){
                case READ_INIT: // MCMDsetModes
                    LOG("reading config init\n");
                    readInit();
                    Setup->init = true;
                    break;
                case READ_MEASURE: // MCMDsetNewTime
                    LOG("reading Meas value\n");
                    readMeas(&readings.timerStep);
                    break;
                default: // MCMDnothing and other unnown commands also READ_NONE
                    LOG("nothing to do");
                    break;
            }
	}

        repeatSCMD = 0;
        if(((uint16_t)(RXbuf[63] + (RXbuf[64]<<8)) != exchg.sentCRC[2]) && 
	              (exchg.sentSCMD != WRITE_NONE)){
            repeatSCMD = exchg.sentSCMD[2];
        }

        // Prepare output data
        if(!initSent || repeatSCMD == WRITE_INIT){
            writeInit();
	    initSent = true;
        }
        else if(Setup->modes.save || repeatSCMD == WRITE_MODES){
            writeModes();
        }
        else
            writeNone();

        uint8_t status = 0;
        if(Setup->modes.save || Setup->user_active){
            status |= 1<<BIT_USER_ACTIVE;
	    Setup->user_active = false;
        }
        if(Setup->wifi_conf.wifi_connected){
            status |= 1<<BIT_WIFI_CONNECTED;
        }
        TXbuf[1] = status;

	// send CRC of data that is transmitted
        uint16_t CRC = crc16(TXbuf, 65);
        TXbuf[65] = (uint8_t)CRC;
        TXbuf[66] = (uint8_t)(CRC>>8);

	exchg.sentCRC[0] = CRC;
	exchg.sentSCMD[0] = TXbuf[0];

        for(int i=2; i>0; i--){
            exchg.sentCRC[i] = exchg.sentCRC[i-1];
	    exchg.sentSCMD[i] = exchg.sentSCMD[i-1];
        }
    }
    LOG("freeing RX/TX buffers\n");
    free(TXbuf);
    free(RXbuf);
}
