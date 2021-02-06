#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "freertos/FreeRTOS.h"
#include "http_server.h"
#include "structures.h"
//#include "constants.h"

#define PART_STR "HTTP helpers: "
#define LOG(str, ... ) printf(PART_STR);\
                       printf(str, ##__VA_ARGS__)

void append_id(char *result, uint16_t value){
    while(result[0] != 0) result++;
    sprintf(result, "%d", value);
}

void append_val(char *result, uint32_t value){
    while(result[0] != 0) result++;
    int ms = value%1000;
    int s = (value/1000)%60;
    int min = (value/60000)%60;
    int h = value/3600000;
    if (h>0)
        sprintf(result, "%d:%d:%d.%03d", h, min, s, ms);
    else{
        if (min>0)
            sprintf(result, "%d:%d.%03d", min, s, ms);
        else
            sprintf(result, "%d.%03d", s, ms);
    }
}

void decode_ip(uint8_t* ip, char* buf){
  int i[4] = {0};
  int id = 0;
  while(id<4 && ((buf[0] >= '0' && buf[0] <= '9') || buf[0]=='.')){
    while(buf[0] >= '0' && buf[0] <= '9'){
      i[id] *= 10;
      i[id] += buf[0] - '0';
      buf++;
      if (i[id] > 255)
        return;
    }
    buf++;
    id++;
  }

  ip[0] = i[0];
  ip[1] = i[1];
  ip[2] = i[2];
  ip[3] = i[3];
}

void decode_mac(uint8_t* ip, char* buf){//00:1D:73:55:2D:1D
  int i[6] = {0};
  int id = 0;
  while(id<6 && ((buf[0] >= '0' && buf[0] <= '9') ||
                 (buf[0] >= 'a' && buf[0] <= 'f') ||
                 (buf[0] >= 'A' && buf[0] <= 'F') ||
                 (buf[0]=='%' && buf[1]=='3' && buf[2]=='A'))){

    if (buf[0]=='%' && buf[1]=='3' && buf[2]=='A')
        buf += 3;

    LOG("MAC readed %c%c\n", buf[0], buf[1]);
    while ((buf[0] >= '0' && buf[0] <= '9') ||
           (buf[0] >= 'a' && buf[0] <= 'f') ||
           (buf[0] >= 'A' && buf[0] <= 'F')) {
      i[id] *= 16;
      if (buf[0] >= '0' && buf[0] <= '9')
          i[id] += buf[0] - '0';
      if (buf[0] >= 'a' && buf[0] <= 'f')
          i[id] += buf[0] - 'a' + 10;
      if (buf[0] >= 'A' && buf[0] <= 'F')
          i[id] += buf[0] - 'A' + 10;
      LOG("%c->%d\n", buf[0], i[id]);
      buf++;
      if (i[id] > 255)
        return;
    }
    LOG(" %d\n", i[id]);
    id++;
  }

  ip[0] = i[0];
  ip[1] = i[1];
  ip[2] = i[2];
  ip[3] = i[3];
  ip[4] = i[4];
  ip[5] = i[5];
}

int s2i(char* buf){
  int ret = 0;
  while(buf[0]>='0' && buf[0]<='9'){
    ret *= 10;
    ret += buf[0] - '0';
    buf++;
  }
  return ret;
}

int s2bcd(char* buf){
  int ret = 0;
  while(buf[0]>='0' && buf[0]<='9'){
    ret *= 16;
    ret += buf[0] - '0';
    buf++;
    LOG("%d\n", ret);
  }
  return ret;
}

