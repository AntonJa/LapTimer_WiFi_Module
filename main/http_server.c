#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "freertos/FreeRTOS.h"
#include "string.h"
#include "fn_defs.h"
#include "structures.h"
#include "http_server.h"
#include "lwip/api.h"
#include "driver/gpio.h"
#include "constants.h"

#define PART_STR "HTTP: "
#define LOG(str, ... ) printf(PART_STR);\
                       printf(str, ##__VA_ARGS__)

/*
 * http_server
 *  |-> http_server_netconn_serve
 *       |-> parse_setup
 *       |-> parse_modes
 *       |-> generate_ajax
 *       |-> generate_csv
 *       |-> generate_html
 *       |    |-> generate_html_head
 *       |    |-> generate_html_body_setup
 *       |    |-> generate_csv
 *       |    |-> generate_csv
 *       |    |-> generate_html_body_main
 */

setup_type *Setup;

void parse_modes(char* buf, http_state *state, char *error){
  bool save_modes_conf = false;
  error[0] = 0;
  while(buf[0]!='?')
    buf++;
  buf++;//mod_en_1=on&mod_lap_gym_1=lap&mod_bikes_1=4&mod_lap_gym_2=gym&mod_bikes_2=4&mod_bikes_3=4 HTTP/1.1

  while(buf[0] != 0 && buf[0] != ' '){
    LOG("start modes\n");
    bool found = false;
    int id;
    int cid;
    for (id = 0; id < setup_modes_str_NR; id++){
      cid = 0;
      while (setup_modes_str[id][cid] != 0){
        if (setup_modes_str[id][cid] != buf[cid])
          break;
        cid++;
      }
      if(setup_modes_str[id][cid] == 0){
        found = true;
        break;
      }
    }

    if (!found){
      while(buf[0]!='&')
        buf++;
      buf++;
      continue;
    }

        buf += cid + 1;// jump after "="
    switch(id+1){
      case enum_modes_mod_lap_gym_1://mod_lap_gym_1=lap
           LOG("mode 0 LapGym\n");
	   if (buf[0] == 'l' && buf[1] == 'a' && buf[2] == 'p')
               Setup->modes.mode[0].lap_gym = false;
           else
               Setup->modes.mode[0].lap_gym = true;
           save_modes_conf = true;
           break;
      case enum_modes_mod_bikes_1://mod_bikes_1=4
           LOG("mode 0 bikes\n");
	   Setup->modes.mode[0].bikes = s2i(buf);
           save_modes_conf = true;
           break;
      case enum_modes_mod_lap_gym_2://mod_lap_gym_2=lap
           LOG("mode 1 LapGym\n");
	   if (buf[0] == 'l' && buf[1] == 'a' && buf[2] == 'p')
               Setup->modes.mode[1].lap_gym = false;
           else
               Setup->modes.mode[1].lap_gym = true;
           save_modes_conf = true;
           break;
      case enum_modes_mod_bikes_2://mod_bikes_2=4
           LOG("mode 1 bikes\n");
	   Setup->modes.mode[1].bikes = s2i(buf);
           save_modes_conf = true;
           break;
      case enum_modes_mod_lap_gym_3://mod_lap_gym_3=lap
           LOG("mode 2 LapGym\n");
	   if (buf[0] == 'l' && buf[1] == 'a' && buf[2] == 'p')
               Setup->modes.mode[2].lap_gym = false;
           else
               Setup->modes.mode[2].lap_gym = true;
           save_modes_conf = true;
           break;
      case enum_modes_mod_bikes_3://mod_bikes_3=4
           LOG("mode 2 bikes\n");
	   Setup->modes.mode[2].bikes = s2i(buf);
           save_modes_conf = true;
           break;
      default:
           break;
    }
    while(buf[0]!='&' && buf[0]!=' ')
      buf++;
    if (buf[0] == '&')
      buf++;//mod_lap_gym_1=lap&mod_bikes_1=4&mod_lap_gym_2=gym&mod_bikes_2=4&mod_bikes_3=4 HTTP/1.1
    LOG("end <%c>\n", buf[0]);
  }
  if(save_modes_conf)
    Setup->modes.save = 1;
}

void parse_setup(char* buf, http_state *state, char *error){
  bool save_wifi_conf = false;
  error[0] = 0;
  while(buf[0]!='?')
    buf++;
  buf++;//type=ap&ip=192.168.0.1&gw=192.168.0.1&mask=255.255.255.0&ssid=MGR&password=12345&channel=5&authmode=open&con_limit=4&ip=0.0.0.0&gw=0.0.0.0&mask=0.0.0.0&ssid=&password=&bssid=0-0-0-0-0-0&dhcp=on&bssid=&bssid=&bssid= HTTP/1.1

  while(buf[0] != 0 && buf[0] != ' '){
    LOG("start\n");
    bool found = false;
    int id;
    int cid;
    int i;
    for (id = 0; id < setup_save_str_NR; id++){
      cid = 0;
      while (setup_save_str[id][cid] != 0){
        if (setup_save_str[id][cid] != buf[cid])
          break;
        cid++;
      }
      if(setup_save_str[id][cid] == 0){
        found = true;
        break;
      }
    }

    if (!found){
      while(buf[0]!='&')
        buf++;
      buf++;
      continue;
    }

    buf += cid + 1;// jump after "="
    int year;

    switch(id+1){

      case enum_setup_ap_ip://ap_ip=192.168.0.1
           decode_ip(Setup->wifi_conf.ap_ip, buf);
           save_wifi_conf = true;
           break;
      case enum_setup_ap_gw://ap_gw=192.168.0.1
           decode_ip(Setup->wifi_conf.ap_gw, buf);
           save_wifi_conf = true;
           break;
      case enum_setup_ap_mask://ap_mask=255.255.255.0
           decode_ip(Setup->wifi_conf.ap_mask, buf);
           save_wifi_conf = true;
           break;
      case enum_setup_ap_ssid://ap_ssid=Lap%20timer
           for (int i = 0; i<32; i++){
             Setup->wifi_conf.ap_conf.ssid[i] = buf[i];
             if(buf[i] == '&'){
               Setup->wifi_conf.ap_conf.ssid[i] = 0;
               break;
             }
           }
           save_wifi_conf = true;
           break;
      case enum_setup_ap_password://ap_password=12345
           for (i = 0; i<32; i++){
             Setup->wifi_conf.ap_conf.password[i] = buf[i];
             if(buf[i] == '&'){
               Setup->wifi_conf.ap_conf.password[i] = 0;
               break;
             }
           }
           if(i<8){
               switch(state->language){
                   case enum_lv://012345678901234
                       strcat(error, "Kļūda: Parolei jābūt vismaz 8 simboliem garai.");
                       break;
                   case enum_en:
                       strcat(error, "Error: password must be at least 8 characters long.");
                       break;
                   case enum_ru:
                       strcat(error, "Ошыбка: пароль должен быть хотябы в 8 символов.");
                       break;
               }
           }
           save_wifi_conf = true;
           break;
      case enum_setup_ap_channel://ap_channel=5
           Setup->wifi_conf.ap_conf.channel = s2i(buf);
           save_wifi_conf = true;
           break;
      case enum_setup_ap_authmode://ap_authmode=open
           Setup->wifi_conf.ap_conf.authmode = buf[0]-'0';
           if(Setup->wifi_conf.ap_conf.authmode == 0)
             error[0] = 0;
           save_wifi_conf = true;
           break;
      case enum_setup_ap_con_limit://ap_con_limit=4
           Setup->wifi_conf.ap_conf.max_connection = s2i(buf);
           save_wifi_conf = true;
           break;
      case enum_setup_ap_hide_ssid://ap_hide_ssid=on
           if (buf[0] == 'o' && buf[1] == 'n')
             Setup->wifi_conf.ap_conf.ssid_hidden = true;
           else
             Setup->wifi_conf.ap_conf.ssid_hidden = false;
           save_wifi_conf = true;
           break;
      case enum_setup_sta_ssid://sta_ssid=
           for (i = 0; i<32; i++){
             Setup->wifi_conf.sta_conf.ssid[i] = buf[i];
             if(buf[i] == '&' || i == 31){
               Setup->wifi_conf.sta_conf.ssid[i] = 0;
               break;
             }
           }
           Setup->wifi_conf.sta_conf.bssid_set = false;
           save_wifi_conf = true;
           break;
      case enum_setup_sta_password://sta_password=
           for (i = 0; i<32; i++){
             Setup->wifi_conf.sta_conf.password[i] = buf[i];
             if(buf[i] == '&' || i == 31){
               Setup->wifi_conf.sta_conf.password[i] = 0;
               break;
             }
           }
           save_wifi_conf = true;
           break;
      case enum_setup_sta_bssid://sta_bssid=0:0:0:0:0:0
           decode_mac(Setup->wifi_conf.sta_conf.bssid, buf);
           save_wifi_conf = true;
           break;
      case enum_setup_sta_use_mac://sta_use_mac=on
           if (buf[0] == 'o' && buf[1] == 'n')
             Setup->wifi_conf.sta_conf.bssid_set = true;
           LOG("MAC filter ON found\n");
           save_wifi_conf = true;
           break;
      case enum_setup_ds_seconds://ds_sec=
           LOG("seconds start\n");
           Setup->ds_conf.www_busy = true;
           while(Setup->ds_conf.i2c_busy){
               // check if both was set at one time
               if(Setup->ds_conf.i2c_waiting){
                   break;
               }
           }
           Setup->ds_conf.bcd_seconds = s2bcd(buf);
           LOG("seconds = %02x\n", Setup->ds_conf.bcd_seconds);
           break;
      case enum_setup_ds_minutes://ds_min=
           Setup->ds_conf.bcd_minutes = s2bcd(buf);
           LOG("minutes = %02x\n", Setup->ds_conf.bcd_minutes);
           break;
      case enum_setup_ds_hours://ds_hur=
           Setup->ds_conf.bcd_hour = s2bcd(buf);
           LOG("hours = %02x\n", Setup->ds_conf.bcd_hour);
           break;
      case enum_setup_ds_date://ds_dat=
           Setup->ds_conf.bcd_date = s2bcd(buf);
           LOG("date = %02x\n", Setup->ds_conf.bcd_date);
           break;
      case enum_setup_ds_month://ds_mon=
           Setup->ds_conf.bcd_month = s2bcd(buf);
           LOG("month = %02x\n", Setup->ds_conf.bcd_month);
           break;
      case enum_setup_ds_year://ds_yer=
           year = s2bcd(buf);
           if(year > 0x2099)
               Setup->ds_conf.century = true;
           else
               Setup->ds_conf.century = false;
           Setup->ds_conf.bcd_year = year & 0xff;
           LOG("year = %02x, century = %d\n", Setup->ds_conf.bcd_year, Setup->ds_conf.century);
           break;
      case enum_setup_ds_aging://ds_age=
           Setup->ds_conf.aging = s2i(buf);
           LOG("agning = %d\n", Setup->ds_conf.aging);
           Setup->ds_conf.changed = true;
           Setup->ds_conf.OSF = false;
           break;
      default:
           break;
    }
    while(buf[0]!='&' && buf[0]!=' ')
      buf++;
    if (buf[0] == '&')
      buf++;//ip=192.168.0.1&gw=192.168.0.1&mask=255.255.255.0&ssid=MGR&password=12345&channel=5&authmode=open&con_limit=4&ip=0.0.0.0&gw=0.0.0.0&mask=0.0.0.0&ssid=&password=&bssid=0-0-0-0-0-0&dhcp=on&bssid=&bssid=&bssid= HTTP/1.1
      LOG("end <%c>\n", buf[0]);
  }
  if(save_wifi_conf){
      nvs_set_wifi_conf();
      esp_restart();
  }
  LOG("time end changed=%d\n", Setup->ds_conf.changed);
  Setup->ds_conf.www_busy = false;
}

void generate_ajax(char *ajax, reading_type* list,int len, http_state *state,int8_t timerStep){
    LOG("AJAX: start processing\n");
    if (list == NULL){
        strcpy(ajax, "\n");
        return;
    }
    strcpy(ajax, "<table>\n");
    LOG("AJAX: selecting language %d\n", state->language);
    switch(state->language){
        case enum_ru:
            strcat(ajax, " <tr>\n"
                         "  <th name=\"run\">№</th>\n"
			 "  <th name=\"type\">Тип</th>\n"
                         "  <th name=\"time\">Время</th>\n"
                         " </tr>\n");
            if (timerStep >= enum_timer_start_0 && timerStep <= enum_timer_start_9){
              strcat(ajax, "  <tr>\n"
                           "    <td colspan=\"3\">СTАРТ</td>\n");
              strcat(ajax, "  </tr>\n");
            }
            if (timerStep >= enum_timer_stop_0 && timerStep <= enum_timer_stop_9){
              strcat(ajax, "  <tr>\n"
                           "    <td colspan=\"3\">ЖДЁМ</td>\n");
              strcat(ajax, "  </tr>\n");
            }
            break;
        case enum_lv:
            strcat(ajax, " <tr>\n"
                         "  <th name=\"run\">Npk</th>\n"
                         "  <th name=\"type\">Tips</th>\n"
                         "  <th name=\"time\">Laiks</th>\n"
                         " </tr>\n");
            if (timerStep >= enum_timer_start_0 && timerStep <= enum_timer_start_9){
              strcat(ajax, "  <tr>\n"
                           "    <td colspan=\"3\">STARTS</td>\n"
                           "  </tr>\n");
            }
            if (timerStep >= enum_timer_stop_0 && timerStep <= enum_timer_stop_9){
              strcat(ajax, "  <tr>\n"
                           "    <td colspan=\"3\">GAIDAM</td>\n"
                           "  </tr>\n");
            }
            break;
        case enum_en:
            strcat(ajax, " <tr>\n"
                         "  <th name=\"run\">#</th>\n"
			 "  <th name=\"type\">Type</th>\n"
                         "  <th name=\"time\">Time</th>\n"
                         " </tr>\n");
            if (timerStep >= enum_timer_start_0 && timerStep <= enum_timer_start_9){
              strcat(ajax, "  <tr>\n"
                           "    <td colspan=\"3\">START</td>\n"
                           "  </tr>\n");
            }
            if (timerStep >= enum_timer_stop_0 && timerStep <= enum_timer_stop_9){
              strcat(ajax, "  <tr>\n"
                           "    <td colspan=\"3\">WAITING</td>\n"
                           "  </tr>\n");
            }
            break;
    }
    //populate list
    LOG("AJAX: populating list\n");
    int max = len;
    if (len > 30) max = 30;
    for (int i = 0; i<max; i++){
        if (list[i].value != 0){
            strcat(ajax, "  <tr>\n"
              "    <td>");
            append_id(ajax, list[i].id);
            strcat(ajax, "</td>\n"
              "    <td>");
            if(list[i].lapGym == 0)
              strcat(ajax, "&#10226; ");
            else
              strcat(ajax, "<u>&#8630;</u> ");
            append_id(ajax, list[i].bykeNr);
	    strcat(ajax, "/");
	    append_id(ajax, list[i].bykes);
	    strcat(ajax, "</td>\n"
              "    <td>");
            if (list[i].referee < 0)
              strcat(ajax, "<s>");
            append_val(ajax, list[i].value);
            if (list[i].referee > 0){
              strcat(ajax, "(+");
              if((list[i].referee % 1000) > 0)
                append_val(ajax, list[i].referee);
	      else
	        append_id(ajax, list[i].referee / 1000);
	      strcat(ajax, ")");
            }
            if (list[i].referee < 0)
              strcat(ajax, "</s>");
            strcat(ajax, "</td>\n"
              "  </tr>\n");
        }
    }
    strcat(ajax, "</table> ");
    LOG("AJAX: ready\n");
}

static void generate_html_head(char *html, int html_len, http_state* state){
    char *P0 = malloc(35);//[35];
    if (P0 == NULL){
      LOG("Error: P0 malloc failed.\n");
      return;
    }
    P0[0] = 0;
    char *P1 = malloc(35);//[35];
    if (P1 == NULL){
      LOG("Error: P1 malloc failed.\n");
      free(P0);
      return;
    }
    P1[0] = 0;
    char *P2 = malloc(35);//[35];
    if (P2 == NULL){
      LOG("Error: P2 malloc failed.\n");
      free(P0);
      free(P1);
      return;
    }
    P2[0] = 0;
    char *P21 = malloc(35);//[35];
    if (P21 == NULL){
      LOG("Error: P21 malloc failed.\n");
      free(P0);
      free(P1);
      free(P2);
      return;
    }
    P21[0] = 0;
    char *P3 = malloc(35);//[35];
    if (P3 == NULL){
      LOG("Error: P3 malloc failed.\n");
      free(P0);
      free(P1);
      free(P2);
      free(P21);
      return;
    }
    P3[0] = 0;
    char *P31 = malloc(95);//[95];
    if (P31 == NULL){
      LOG("Error: P31 malloc failed.\n");
      free(P0);
      free(P1);
      free(P2);
      free(P21);
      free(P3);
      return;
    }
    P31[0] = 0;
    char *P41 = malloc(95);//[95];
    if (P41 == NULL){
      LOG("Error: P41 malloc failed.\n");
      free(P0);
      free(P1);
      free(P2);
      free(P21);
      free(P3);
      free(P31);
      return;
    }
    P41[0] = 0;
    char *P5 = malloc(35);//[35];
    if (P5 == NULL){
      LOG("Error: P5 malloc failed.\n");
      free(P0);
      free(P1);
      free(P2);
      free(P21);
      free(P3);
      free(P31);
      free(P41);
      return;
    }
    P5[0] = 0;
    char ru[] = "2";
    char lv[] = "2";
    char en[] = "2";
    char eMain[] = "2"; 
    char eSetup[] = "2";
    char eModes[] = "2";
    //mobile parameters
    char mMain[] = "755px";
    char mTlH[] = " 8";
    char mTlW[] = "16";
    char mThinH[] = " 8";
    char mThinW[] = "7";
    char mT1[] = "6";
    char mT9[] = "7";
    char mFormTableW[] = "575px";
    char mTableF[] = "2";
    char mMenuW[] = "176";
    char mSetupText[] = "1";
    char mCheckSize[] = "2";
    char mTxtW[] = "2";
    char mTxtM[] = "2";

    if (state->mobile){
        mMain[0] = '9';
        mTlH[0] = '1';
        mTlH[1] = '0';
        mTlW[0] = '2';
        mTlW[1] = '0';
        mThinH[0] = '1';
        mThinH[1] = '0';
        mThinW[0] = '9';
        mT1[0] = '9';
        mT9[0] = '9';
        mFormTableW[0] = '7';
        mFormTableW[1] = '3';
        mFormTableW[2] = '9';
        mTableF[0] = '3';
        mMenuW[0] = '2';
        mMenuW[1] = '1';
        mMenuW[2] = '4';
        mSetupText[0] = '2';
        mCheckSize[0] = '4';
        mTxtW[0] = '3';
        mTxtM[0] = '4';
    }

    switch(state->page){
        case enum_setup:
            eSetup[0] = '1';
            break;
	case enum_modes:
            eModes[0] = '1';
	    break;
        case enum_monitor:
        case enum_referee:
            mMain[0] = ' ';
            mMain[1] = '1';
            mMain[2] = '0';
            mMain[3] = '0';
            mMain[4] = '%';
            mFormTableW[0] = ' ';
            mFormTableW[1] = '1';
            mFormTableW[2] = '0';
            mFormTableW[3] = '0';
            mFormTableW[4] = '%';
            break;
        default:
            eMain[0] = '1';
            break;
    }
    
    switch(state->language){
        case enum_lv://012345678901234
            strcpy(P0,"Braucieni");
            strcpy(P1,"Monitora režīms");
            strcpy(P5,"Tiesneša režīms");
            strcpy(P2,"Iestat.");
	    strcpy(P21,"Režīms");
            strcpy(P3,"Ielādēt");
            strcpy(P31,"Formāts: .CSV</br>Sadalītājs: semikols");
            strcpy(P41,"Formāts: .CSV</br>Sadalītājs: komats");
            ru[0] = '2';
            lv[0] = '1';
            en[0] = '2';
            break;
        case enum_en:
            strcpy(P0,"Runs");
            strcpy(P1,"Monitor mode");
            strcpy(P5,"Referee mode");
            strcpy(P2,"Setup");
	    strcpy(P21,"Mode");
            strcpy(P3,"Get data");
            strcpy(P31,"Format: .CSV</br>Separator: semicolon");
            strcpy(P41,"Format: .CSV</br>Separator: comma");
            ru[0] = '2';
            lv[0] = '2';
            en[0] = '1';
            break;
        default:
            strcpy(P0,"Заезды");
            strcpy(P1,"Режим монитора");
            strcpy(P5,"Режим судьи");
            strcpy(P2,"Настройка");
	    strcpy(P21,"Режимы");
            strcpy(P3,"Загрузить");
            strcpy(P31,"Формат: .CSV</br>Разделитель: точка с запятой");
            strcpy(P41,"Формат: .CSV</br>Разделитель: запятая");
            ru[0] = '1';
            lv[0] = '2';
            en[0] = '2';
            break;
    }

    strcpy(html, "<!DOCTYPE html>\n"
                 "<html><head>\n"
                 "<meta charset=\"UTF-8\">\n"
                 "<title>Lap timer</title>\n"
                 "<style>\n"
                 "BODY {color:#4F473D;font-family:Verdana,sans-serif;"
                 "font-weight:bold;margin:2px;padding:2px;}\n"
                 ".main {width:");
    strcat(html, mMain);
    strcat(html, ";margin:0 auto;}\n"
                 ".hdr {float:left;width:440px;overflow:auto;}\n"
                 ".tme {float:right;padding-top:30px;margin-right:"
                 "8px;font-size:70%;text-align:right;}\n"
                 ".tl {height:");
    strcat(html, mTlH);
    strcat(html, "0px;width:");
    strcat(html, mTlW);
    strcat(html, "0px;float:left;margin-bottom:4px;margin-left:4px;"
                 "padding-left:10px;color:#FFFFFF;}\n"
                 ".thin {width:");
    strcat(html, mThinW);
    strcat(html, "3px;height:");
    strcat(html, mThinH);
    strcat(html, "0px;}\n"
                 ".thin2 {width:73px;height: 80px;font-size:150%;}\n"
                 ".no0 {background:#F2F2F2;}\n"
                 ".no1 {background:#f58220;}\n"
                 ".no2 {background:#F2F2F2;color:#4F473D;}\n"
                 ".no2:hover {background:#D0E5FA;color:#4F473D;}\n"
                 ".t1 {font-size:1");
    strcat(html, mT1);
    strcat(html, "0%;margin-top:10px;display:block;}\n"
                 ".t5 {font-size:80%;margin-top:-22px;"
                 "margin-left:4px;margin-bottom:10px;display:block;}\n"
                 ".t9 {font-size:");
    strcat(html, mT9);
    strcat(html, "0%;margin-top:-2px;font-weight:normal;display:block;}\n"
                 "h1 {font-size:180%;}\n"
                 "A {text-decoration:none;}\n"
                 "form {width:");
    strcat(html, mFormTableW);
    strcat(html, "px;float:right;}\n"
                 "button {float:left;font-size:");
    strcat(html, mSetupText);
    strcat(html, "00%;}\n"
                 ".cButton {float:left;margin-bottom:4px;margin-left:4px;"
                 "padding-left:10px;color:#4F473D;width:73px;height: 80px;"
                 "font-size:140%;border:none;background:#F2F2F2;text-align:left;}\n"
                 ".cButton:hover {background:#D0E5FA;color:#4F473D;}\n"
                 ".cButton:disabled {background:#F2F2F2;color:#AFAFAF;}\n"
                 ".inform {width:");
    strcat(html, mFormTableW);
    strcat(html, "px;float:left;}\n"
                 ".clear {clear:both;}\n"
                 ".txt {width:");
    strcat(html, mTxtW);
    strcat(html, "00px;float:left;margin-bottom:");
    strcat(html, mTxtM);
    strcat(html, "0px;font-size:");
    strcat(html, mSetupText);
    strcat(html, "00%;}\n"
                 "select {font-size:");
    strcat(html, mSetupText);
    strcat(html, "00%;}\n"
                 "input {width:250px;border:solid 1px;font-size:");
    strcat(html, mSetupText);
    strcat(html, "00%;}\n"
                 "input[type=\"checkbox\"]{width:");
    strcat(html, mCheckSize);
    strcat(html, "0px;height:");
    strcat(html, mCheckSize);
    strcat(html, "0px;border:solid 1px;font-size:");
    strcat(html, mSetupText);
    strcat(html, "00%;}\n"
                 "input[type=\"radio\"]{width:");
    strcat(html, mCheckSize);
    strcat(html, "0px;height:");
    strcat(html, mCheckSize);
    strcat(html, "0px;border:solid 1px;font-size:");
    strcat(html, mSetupText);
    strcat(html, "00%;}\n"
                 "table {width:");
    strcat(html, mFormTableW);
    strcat(html, ";float:right;font-size:");
    strcat(html, mTableF);
    strcat(html, "00%;}\n"
                 "th, td {text-align:left;padding:8px;}\n");
    if(state->page != enum_referee){
        strcat(html,"tr:nth-child(even){background-color: #F2F2F2;}\n");
    }
    strcat(html, "th {background-color:#f58220;color:white;}\n"
                 ".selected {background-color: brown;color: #FFF;}\n"
                 "</style>\n"
                 "</head>\n"
     "<body onload=\"init();\">\n");
    if(state->page == enum_referee){
        //strcat(html,"<script src=\"http://"
        //"ajax.googleapis.com/ajax/libs/jquery/1.8.3/jquery.min.js\"></script>\n");
        strcat(html,"<script src=\"jquery.min.js\"></script>\n"); // REF_JSON
    }
    strcat(html, "<script type=\"text/javascript\">\n"
                 "function init() {\n"
                 " if(\"\".length!=0){set(\"user\",\"\");}reload();");
    if(state->page == enum_referee){
        strcat(html, "\n  document.getElementById(\"bRes\").disabled = true;\n"
                     "  document.getElementById(\"bDel\").disabled = true;\n"
                     "  document.getElementById(\"bP1\").disabled = true;\n"
                     "  document.getElementById(\"bPx\").disabled = true;");
    }
    strcat(html, "}\n"
                 "function set(id,val){\n"
                 " document.getElementById(id).innerHTML = val;}\n"
                 "var val;\n"
                 "var bEdit=false;\n"
                 "var fn;\n"
                 "var rlds=0;\n"
                 "function reload(){\n");
    if(state->page == enum_referee){
        strcat(html, " if(bEdit)\n"
                     "  return;\n");
    }
    if(state->page == enum_main || state->page == enum_monitor || state->page == enum_referee){
        strcat(html, " var xhttp = new XMLHttpRequest();\n"
                     " xhttp.onreadystatechange = function() {\n"
                     "  if (this.readyState == 4 && this.status == 200) {\n"
                     "   set(\"list\",this.responseText);}};\n"
                     " xhttp.open(\"GET\",\"ajax\",true);\n"
                     " xhttp.send();\n");
    }
    strcat(html, " var xhttp2 = new XMLHttpRequest();\n"
                 " xhttp2.onreadystatechange = function() {\n"
                 "  if (this.readyState == 4 && this.status == 200) {\n"
                 "   set(\"time\",this.responseText);}};\n"
                 " xhttp2.open(\"GET\",\"clock\",true);\n"
                 " xhttp2.send();\n");
    //if((Setup->wifi_conf.save >= 1) && (Setup->wifi_conf.save <= 5) && state->page == enum_setup){
    //    strcat(html, " if(rlds>0)window.location = \"setup\";\n");
    //}
    strcat(html, " setTimeout(reload, 1000);\n"
                 " rlds=rlds+1;}\n"
                 "function edit(){\n"
                 " if(bEdit){\n"
                 "  bEdit=false;\n"
                 "  document.getElementById(\"bRes\").disabled = true;\n"
                 "  document.getElementById(\"bDel\").disabled = true;\n"
                 "  document.getElementById(\"bP1\").disabled = true;\n"
                 "  document.getElementById(\"bPx\").disabled = true;\n"
                 "  reload();}\n"
                 " else{\n"
                 "  bEdit=true;\n"
                 "  document.getElementById(\"bRes\").disabled = false;\n"
                 "  document.getElementById(\"bDel\").disabled = false;\n"
                 "  document.getElementById(\"bP1\").disabled = false;\n"
                 "  document.getElementById(\"bPx\").disabled = false;\n"
                 "  val=undefined;\n"
                 "  fn();}}\n"
                 "function a(){alert(\"Please select Run.\");}\n"
                 "function reset(){\n"
                 " if(val==undefined){\n"
                 "  a();return;}\n"
                 " var xhttp = new XMLHttpRequest();\n"
                 " xhttp.open(\"GET\",\"reset\"+val,true);\n"
                 " xhttp.send();}\n"
                 "function del(){\n"
                 " if(val==undefined){\n"
                 "  a();return;}\n"
                 " var xhttp = new XMLHttpRequest();\n"
                 " xhttp.open(\"GET\",\"delete\"+val,true);\n"
                 " xhttp.send();}\n"
                 "function plus(sec){\n"
                 " if(val==undefined){\n"
                 "  a();return;}\n"
                 " var xhttp = new XMLHttpRequest();\n"
                 " if(sec=='x') var add=document.getElementById(\"iAdd\").value;\n"
                 " else var add=sec;\n"
                 " xhttp.open(\"GET\",\"plus\"+val+\"_\"+add,true);\n"
                 " xhttp.send();}\n"
                 "</script>\n"
                 "<div class=\"main\">\n"
                 "<div class=\"hdr\"><h1><a href=\"/\">Lap timer</a></h1>");
    if (state->page != enum_monitor && state->page != enum_referee){
        strcat(html, "<div class=\"t5\">s/n: 0005</div>");
    }
    strcat(html, "</div>\n"
                 "<div class=\"tme\"><span id=\"time\"></span>"
                 "<br><span id=\"user\"></span></div>\n"
                 "<div class=\"clear\"></div>\n");
    if(state->page == enum_referee){
        strcat(html, " <button class=\"cButton\" onclick=\"edit()\">"
                     "<span class=\"t1\"><b>✎</b></span></button>\n"
                     " <button class=\"cButton\" onclick=\"reset()\" id=\"bRes\">"
                     "<span class=\"t1\"><b>↶</b></span></button>\n"
                     " <button class=\"cButton\" onclick=\"del()\" id=\"bDel\">"
                     "<span class=\"t1\"><s>xx</s></span></button>\n"
                     " <button class=\"cButton\" onclick=\"plus(1)\" id=\"bP1\">"
                     "<span class=\"t1\"><b>+1s</b></span></button>\n"
                     " <input class=\"thin2\" type=\"text\" id=\"iAdd\" "
                     "maxlength=\"32\" style=\"margin-left:4px;height: 76px;float:left\">\n"
                     " <button class=\"cButton\" onclick=\"plus('x')\" id=\"bPx\">"
                     "<span class=\"t1\"><b>+</b></span></button>\n"
                     "<div class=\"clear\"></div>\n");
    }
    if (state->page != enum_monitor && state->page != enum_referee){
        strcat(html, "<div style=\"width:");
        strcat(html, mMenuW);
        strcat(html, "px;float:left;\">\n"
                     " <a href=\"main\" class=\"tl no");
        strcat(html, eMain);
        strcat(html, "\"><span class=\"t1\">");
        strcat(html, P0);
        strcat(html, "</span></a>\n"
                     " <a href=\"monitor\" class=\"tl no2\"><span class=\"t1\">");
        strcat(html, P1);
        strcat(html, "</span></a>\n"
                     " <a href=\"referee\" class=\"tl no2\"><span class=\"t1\">");
        strcat(html, P5);
        strcat(html, "</span></a>\n"
                     " <a href=\"setup\" class=\"tl no");
        strcat(html, eSetup);
        strcat(html, "\"><span class=\"t1\">");
        strcat(html, P2);
        strcat(html, "</span><span class=\"t9\">WiFi: ");
        if (Setup->wifi_conf.wifi_mode)//0=Access Point, 1=WiFi station
            strcat(html, "STA");
        else
            strcat(html, "AP");
        strcat(html, "</span><span class=\"t9\">SSID: ");
        if (Setup->wifi_conf.wifi_mode)//0=Access Point, 1=WiFi station
            strcat(html, (char*)Setup->wifi_conf.sta_conf.ssid);
        else
            strcat(html, (char*)Setup->wifi_conf.ap_conf.ssid);


	strcat(html, "</span></a>\n"
                     " <a href=\"modes\" class=\"tl no");
        strcat(html, eModes);
        strcat(html, "\"><span class=\"t1\">");
        strcat(html, P21);


        strcat(html, "</span></a>\n"
                     " <a href=\"ru\" class=\"tl thin no");
        strcat(html, ru);
        strcat(html, "\"><span class=\"t1\">RU</span></a>\n"
                     " <a href=\"lv\" class=\"tl thin no");
        strcat(html, lv);
        strcat(html, "\"><span class=\"t1\">LV</span></a>\n"
                     " <a href=\"en\" class=\"tl thin no");
        strcat(html, en);
        strcat(html, "\"><span class=\"t1\">EN</span></a>\n"
                     " <div class=\"tl thin no0\"></div>\n"
                     " <a href=\"GetCol.csv\" class=\"tl no2\"><span class=\"t1\">");
        strcat(html, P3);
        strcat(html, "</span><span class=\"t9\">");
        strcat(html, P31);
        strcat(html, "</span></a>\n"
                     " <a href=\"GetComa.csv\" class=\"tl no2\"><span class=\"t1\">");
        strcat(html, P3);
        strcat(html, "</span><span class=\"t9\">");
        strcat(html, P41);
        strcat(html, "</span></a>\n"
                     "</div>\n");
    }
    free(P0);
    free(P1);
    free(P2);
    free(P21);
    free(P3);
    free(P31);
    free(P41);
    free(P5);
}

static void generate_html_body_main(char *html, int html_len, http_state* state){
    strcat(html, "<div id=\"list\">\n"
                 "</div>\n");
    if(state->page == enum_referee){
        strcat(html, "<script>\n"
                     "$(function(){\n"
                     " function setActiveRows(){\n"
                     "  $('table tr').click(function() {\n"
                     "   $(this).addClass('selected').siblings().removeClass('selected');\n"
                     "   //var value = $(this).find('td:first').html();\n"
                     "   val = $(this).find('td:first').html();\n"
                     "  });\n"
                     " };\n"
                     " setActiveRows();\n"
                     " fn=setActiveRows;});\n"
                     "</script>\n");
    }
}

static void generate_html_body_setup(char *html, int html_len, http_state* state, char *error){
    char channel[5];
    itoa(Setup->wifi_conf.ap_conf.channel, channel, 10);

    char con_limit[2] = "0";
    con_limit[0] += Setup->wifi_conf.ap_conf.max_connection;

    char bssid[18];
    sprintf(bssid,
            "%X:%X:%X:%X:%X:%X",
            Setup->wifi_conf.sta_conf.bssid[0],
            Setup->wifi_conf.sta_conf.bssid[1],
            Setup->wifi_conf.sta_conf.bssid[2],
            Setup->wifi_conf.sta_conf.bssid[3],
            Setup->wifi_conf.sta_conf.bssid[4],
            Setup->wifi_conf.sta_conf.bssid[5]);

    char ap_ip[16];
    sprintf(ap_ip,"%d.%d.%d.%d",Setup->wifi_conf.ap_ip[0],Setup->wifi_conf.ap_ip[1],
                                Setup->wifi_conf.ap_ip[2],Setup->wifi_conf.ap_ip[3]);

    char ap_gw[16];
    sprintf(ap_gw,"%d.%d.%d.%d",Setup->wifi_conf.ap_gw[0],Setup->wifi_conf.ap_gw[1],
                                Setup->wifi_conf.ap_gw[2],Setup->wifi_conf.ap_gw[3]);

    char ap_mask[16];
    sprintf(ap_mask,"%d.%d.%d.%d",Setup->wifi_conf.ap_mask[0],Setup->wifi_conf.ap_mask[1],
                                  Setup->wifi_conf.ap_mask[2],Setup->wifi_conf.ap_mask[3]);

   LOG("body init1 done\n");

    char ds_acpo[40] = {0};
    char ds_clie[40] = {0};
    char ds_rset[40] = {0};
    char ds_seco[40] = {0};
    char ds_minu[40] = {0};
    char ds_hour[40] = {0};
    char ds_date[40] = {0};
    char ds_mont[40] = {0};
    char ds_year[40] = {0};
    char ds_agin[40] = {0};
    char ds_save[20] = {0};
    char ds_savi[26] = {0};
    switch(state->language){
        case enum_lv:     //012345678901234567890
            strcpy(ds_acpo,"Piekļuves punkts");
            strcpy(ds_clie,"Klients");
            strcpy(ds_rset,"Pulk. bija atslēgts!");
            strcpy(ds_seco,"Pulk. sekundes:");
            strcpy(ds_minu,"Pulk. minūtes:");
            strcpy(ds_hour,"Pulk. stundas:");
            strcpy(ds_date,"Pulk. datums:");
            strcpy(ds_mont,"Pulk. mēnesis:");
            strcpy(ds_year,"Pulk. gads:");
            strcpy(ds_agin,"Pulk. novecošana:");
            strcpy(ds_save,"Saglabāt");
            strcpy(ds_savi,"Saglabā...");
            break;
        case enum_en:
            strcpy(ds_acpo,"Access point");
            strcpy(ds_clie,"Client");
            strcpy(ds_rset,"Clock was stopped!");
            strcpy(ds_seco,"Clock seconds:");
            strcpy(ds_minu,"Clock minutes:");
            strcpy(ds_hour,"Clock hours:");
            strcpy(ds_date,"Clock date:");
            strcpy(ds_mont,"Clock month:");
            strcpy(ds_year,"Clock year:");
            strcpy(ds_agin,"Clock aging:");
            strcpy(ds_save,"Save");
            strcpy(ds_savi,"Saving...");
            break;
        default:
            strcpy(ds_acpo,"Точка достпа");
            strcpy(ds_clie,"Клиент");
            strcpy(ds_rset,"Часы были сброшенны!");
            strcpy(ds_seco,"Секунды часов:");
            strcpy(ds_minu,"Минуты часов:");
            strcpy(ds_hour,"Часы часов:");
            strcpy(ds_date,"Дата часов:");
            strcpy(ds_mont,"Месяц часов:");
            strcpy(ds_year,"Год часов:");
            strcpy(ds_agin,"Устаревание часов:");
            strcpy(ds_save,"Сохранить");
            strcpy(ds_savi,"Сохраняет...");
            break;
    }

    LOG("body init2 done\n");

    if (!Setup->wifi_conf.wifi_mode){ //AP
        strcat(html, " <a href=\"ap\" class=\"tl no1\">"
                     "<span class=\"t1\">");
        strcat(html, ds_acpo);
        strcat(html, "</span></a>\n"
                     " <a href=\"sta\" class=\"tl no2\">"
                     "<span class=\"t1\">");
        strcat(html, ds_clie);
        strcat(html, "</span></a>\n");
        if (error[0] != 0){
            strcat(html, "<br><br><br><br><br>\n"
                         "<font color=\"red\"><b>");
            strcat(html, error);
            strcat(html, "</b></font>");
        }
        strcat(html, "<form id=\"form\">\n"
                     " <div class=\"txt\">IP:</div>\n"
                     " <input type=\"text\" name=\"ap_ip\" maxlength=\"15\" value=\"");
        strcat(html, ap_ip);
        strcat(html, "\">\n"
                     " <div class=\"clear\"></div>\n"
                     " <div class=\"txt\">Gate Way:</div>\n"
                     " <input type=\"text\" name=\"ap_gw\" maxlength=\"15\" value=\"");
        strcat(html, ap_gw);
        strcat(html, "\">\n"
                     " <div class=\"clear\"></div>\n"
                     " <div class=\"txt\">Mask:</div>\n"
                     " <input type=\"text\" name=\"ap_mask\" maxlength=\"15\" value=\"");
        strcat(html, ap_mask);
        strcat(html, "\">\n"
                     " <div class=\"clear\"></div>\n"
                     " <div class=\"txt\">WiFi name:</div>\n"
                     " <input type=\"text\" name=\"ap_ssid\" maxlength=\"32\" value=\"");
        strcat(html, (char*)Setup->wifi_conf.ap_conf.ssid);
        strcat(html, "\">\n"
                     " <div class=\"clear\"></div>\n"
                     " <div class=\"txt\">Password:</div>\n"
                     " <input type=\"password\" name=\"ap_password\" maxlength=\"64\" value=\"");
        strcat(html, (char*)Setup->wifi_conf.ap_conf.password);
        strcat(html, "\">\n"
                     " <div class=\"clear\"></div>\n"
                     " <div class=\"txt\">Channel:</div>\n"
                     " <input type=\"number\" name=\"ap_channel\" min=\"0\" max=\"15\" value=\"");
        strcat(html, channel);
        strcat(html, "\">\n"
                     " <div class=\"clear\"></div>\n"
                     " <div class=\"txt\">Auth. mode:</div>\n"
                     " <select name=\"ap_authmode\">\n"
                     "  <option ");
        if (Setup->wifi_conf.ap_conf.authmode == WIFI_AUTH_OPEN) //OPEN
            strcat(html, "selected=\"selected\" ");
        strcat(html, "value=\"0\">Open</option>\n"
                     "  <option ");
        if (Setup->wifi_conf.ap_conf.authmode == WIFI_AUTH_WEP) //WEP
            strcat(html, "selected=\"selected\" ");
        strcat(html, "value=\"1\">WEP</option>\n"
                     "  <option ");
        if (Setup->wifi_conf.ap_conf.authmode == WIFI_AUTH_WPA_PSK) //WPA PSK
            strcat(html, "selected=\"selected\" ");
        strcat(html, "value=\"2\">WPA PSK</option>\n"
                     "  <option ");
        if (Setup->wifi_conf.ap_conf.authmode == WIFI_AUTH_WPA2_PSK) //WPA2 PSK
            strcat(html, "selected=\"selected\" ");
        strcat(html, "value=\"3\">WPA2 PSK</option>\n"
                     "  <option ");
        if (Setup->wifi_conf.ap_conf.authmode == WIFI_AUTH_WPA_WPA2_PSK) //WPA WPA2 PSK
            strcat(html, "selected=\"selected\" ");
        strcat(html, "value=\"4\">WPA WPA2 PSK</option>\n"
                     "  <option ");
        if (Setup->wifi_conf.ap_conf.authmode == WIFI_AUTH_WPA2_ENTERPRISE) //WPA2 ENTERPRISE
            strcat(html, "selected=\"selected\" ");
        strcat(html, "value=\"5\">WPA2 ENTERPRISE</option>\n"
                     " </select>\n"
                     " <div class=\"clear\"></div>\n"
                     " <input type=\"checkbox\" name=\"ap_hide_ssid\" ");
        if (Setup->wifi_conf.ap_conf.ssid_hidden != 0) //Hide SSID
            strcat(html, "checked");
        strcat(html, ">\n"
                     " <div class=\"txt\">Hide WiFi name</div>\n"
                     " <div class=\"clear\"></div>\n"
                     " <div class=\"txt\">Connection limit:</div>\n"
                     " <input type=\"number\" name=\"ap_con_limit\" min=\"1\" max=\"4\" value=\"");
        strcat(html, con_limit);
        strcat(html, "\"");
    }
    if (Setup->wifi_conf.wifi_mode){ //STA
        strcat(html, " <a href=\"ap\" class=\"tl no2\">"
                     "<span class=\"t1\">");
        strcat(html, ds_acpo);
        strcat(html, "</span></a>\n"
                     " <a href=\"sta\" class=\"tl no1\"><span class=\"t1\">");
        strcat(html, ds_clie);
        strcat(html, "</span></a>\n");
        if (error[0] != 0){
            strcat(html, "<br><br><br><br><br>\n"
                         "<font color=\"red\"><b>");
            strcat(html, error);
            strcat(html, "</b></font>");
        }
        strcat(html, "<form id=\"form2\">\n"
                     " <div class=\"txt\">WiFi name:</div>\n"
                     " <input type=\"text\" name=\"sta_ssid\" maxlength=\"32\" value=\"");
        strcat(html, (char*)Setup->wifi_conf.sta_conf.ssid); //SSID
        strcat(html, "\">\n"
                     " <div class=\"clear\"></div>\n"
                     " <div class=\"txt\">Password:</div>\n"
                     " <input type=\"password\" name=\"sta_password\" maxlength=\"64\" value=\"");
        strcat(html, (char*)Setup->wifi_conf.sta_conf.password); //Password
        strcat(html, "\">\n"
                     " <div class=\"clear\"></div>\n"
                     " <div class=\"txt\">Target MAC address:</div>\n"
                     " <input type=\"text\" name=\"sta_bssid\" maxlength=\"17\" value=\"");
        strcat(html, bssid); //BSSID
        strcat(html, "\">\n"
                     " <div class=\"clear\"></div>\n"
                     " <div class=\"txt\">Use MAC filter</div>\n"
                     " <input type=\"checkbox\" name=\"sta_use_mac\" ");
        if (Setup->wifi_conf.sta_conf.bssid_set) //use MAC address
            strcat(html, "checked");
    }
    // common part of Setup screen
    strcat(html, ">\n"
                 " <div class=\"clear\"></div>\n"
                 " <button type=\"submit\">");
    //if((Setup->wifi_conf.save >= 1) && (Setup->wifi_conf.save <= 5))
    //    strcat(html, ds_savi);
    //else
        strcat(html, ds_save);
    strcat(html, "</button>\n"
                 "</form>\n");
    // Form for clock setup
    sprintf(channel,"%02x",Setup->ds_conf.bcd_seconds);
    strcat(html, "<form id=\"form3\">\n");
    if(Setup->ds_conf.OSF){
        strcat(html, " <div class=\"clear\"></div>\n"
                     " <div class=\"txt\"><font color=\"red\"><b>");
        strcat(html, ds_rset);
        strcat(html, "</b></font></div>\n");
    }
    strcat(html, " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">");
    strcat(html, ds_seco);
    strcat(html, "</div>\n"
                 " <input type=\"text\" name=\"ds_sec\" maxlength=\"2\" value=\"");
    strcat(html, channel); //Seconds
    sprintf(channel,"%02x",Setup->ds_conf.bcd_minutes);
    strcat(html, "\">\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">");
    strcat(html, ds_minu);
    strcat(html, "</div>\n"
                 " <input type=\"text\" name=\"ds_min\" maxlength=\"2\" value=\"");
    strcat(html, channel); //Minutes
    sprintf(channel,"%02x",Setup->ds_conf.bcd_hour);
    strcat(html, "\">\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">");
    strcat(html, ds_hour);
    strcat(html, "</div>\n"
                 " <input type=\"text\" name=\"ds_hur\" maxlength=\"2\" value=\"");
    strcat(html, channel); //Hours
    sprintf(channel,"%02x",Setup->ds_conf.bcd_date);
    strcat(html, "\">\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">");
    strcat(html, ds_date);
    strcat(html, "</div>\n"
                 " <input type=\"text\" name=\"ds_dat\" maxlength=\"2\" value=\"");
    strcat(html, channel); //date
    sprintf(channel,"%02x",Setup->ds_conf.bcd_month);
    strcat(html, "\">\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">");
    strcat(html, ds_mont);
    strcat(html, "</div>\n"
                 " <input type=\"text\" name=\"ds_mon\" maxlength=\"2\" value=\"");
    strcat(html, channel); //month
    if(Setup->ds_conf.century)
        sprintf(channel,"21%02x",Setup->ds_conf.bcd_year);
    else
        sprintf(channel,"20%02x",Setup->ds_conf.bcd_year);
    strcat(html, "\">\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">");
    strcat(html, ds_year);
    strcat(html, "</div>\n"
                 " <input type=\"text\" name=\"ds_yer\" maxlength=\"4\" value=\"");
    strcat(html, channel); //year
    char aging[5];
    sprintf(aging,"%d",Setup->ds_conf.aging);
    strcat(html, "\">\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">");
    strcat(html, ds_agin);
    strcat(html, "</div>\n"
                 " <input type=\"text\" name=\"ds_age\" maxlength=\"4\" value=\"");
    strcat(html, aging); //aging
    strcat(html, "\">\n"
                 " <div class=\"clear\"></div>\n"
                 " <button type=\"submit\">");
    strcat(html, ds_save);
    strcat(html, "</button>\n"
                 "</form>\n");
}

static void generate_html_body_modes(char *html, int html_len, http_state* state, char *error){
    char mod_mode[30] = {0};
    char mod_lap_mode[30] = {0};
    char mod_gym_mode[30] = {0};
    char mod_bikes[30] = {0};
    char mod_save[20] = {0};
    char mod_bikes_nr[10] = {0};
    char mod_lap_char[18] = {0};
    char mod_gym_char[18] = {0};

    if(Setup->modes.mode[0].bikes < 1)
        Setup->modes.mode[0].bikes = 1;
    if(Setup->modes.mode[1].bikes < 1)
        Setup->modes.mode[1].bikes = 1;
    if(Setup->modes.mode[2].bikes < 1)
        Setup->modes.mode[2].bikes = 1;

    strcpy(mod_gym_char, "(<u>&#8630;</u>)");
    strcpy(mod_lap_char, "(&#10226;)");

    switch(state->language){
        case enum_lv:      //     012345678901
            strcpy(mod_mode,     "Režīms");
            strcpy(mod_lap_mode, "Apļa režīms ");
            strcpy(mod_gym_mode, "Gym. režīms ");
            strcpy(mod_bikes,    "Motocikli:");
	    strcpy(mod_save,     "Saglabāt");
            break;
        case enum_en:
            strcpy(mod_mode,     "Mode");
            strcpy(mod_lap_mode, "Lap mode ");
            strcpy(mod_gym_mode, "Gym. mode ");
            strcpy(mod_bikes,    "Bikes:");
	    strcpy(mod_save,     "Save");
            break;
        default:
            strcpy(mod_mode,     "Режим");
            strcpy(mod_lap_mode, "Режим круга ");
            strcpy(mod_gym_mode, "Режим Gym. ");
            strcpy(mod_bikes,    "Мотоциклы:");
	    strcpy(mod_save,     "Сохранить");
            break;
    }

    strcat(html, "<form id=\"form4\">\n"
		 " <div class=\"txt\">");
    strcat(html, mod_mode);
    strcat(html, " 1</div>\n"
                 " <div class=\"clear\"></div>\n"
		 " <div class=\"txt\">&#8627;&nbsp;");
    strcat(html, mod_lap_mode);
    strcat(html, mod_lap_char);
    strcat(html, ":</div>\n"
                 " <input type=\"radio\" name=\"mod_lap_gym_1\" value=\"lap\"");
    if (!Setup->modes.mode[0].lap_gym)
        strcat(html, " checked");
    strcat(html, ">\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">&#8627;&nbsp;");
    strcat(html, mod_gym_mode);
    strcat(html, mod_gym_char);
    strcat(html, ":</div>\n"
                 " <input type=\"radio\" name=\"mod_lap_gym_1\" value=\"gym\"");
    if (Setup->modes.mode[0].lap_gym)
        strcat(html, " checked");
    strcat(html, ">\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">&nbsp;&nbsp;&#8627;&nbsp;");
    strcat(html, mod_bikes);
    strcat(html, "</div>\n"
                 " <input type=\"number\" name=\"mod_bikes_1\" min=\"1\" max=\"10\" value=\"");
    sprintf(mod_bikes_nr, "%d", Setup->modes.mode[0].bikes);
    strcat(html, mod_bikes_nr);
    strcat(html, "\">\n"
                 " <div class=\"clear\"></div>\n"
		 " <hr>\n"
                 " <div class=\"txt\">");
    strcat(html, mod_mode);
    strcat(html, " 2</div>\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">&#8627;&nbsp;");
    strcat(html, mod_lap_mode);
    strcat(html, mod_lap_char);
    strcat(html, "</div>\n"
                 " <input type=\"radio\" name=\"mod_lap_gym_2\" value=\"lap\"");
    if (!Setup->modes.mode[1].lap_gym)
        strcat(html, " checked");
    strcat(html, ">\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">&#8627;&nbsp;");
    strcat(html, mod_gym_mode);
    strcat(html, mod_gym_char);
    strcat(html, "</div>\n"
                 " <input type=\"radio\" name=\"mod_lap_gym_2\" value=\"gym\"");
    if (Setup->modes.mode[1].lap_gym)
        strcat(html, " checked");
    strcat(html, ">\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">&nbsp;&nbsp;&#8627;&nbsp;");
    strcat(html, mod_bikes);
    strcat(html, "</div>\n"
                 " <input type=\"number\" name=\"mod_bikes_2\" min=\"1\" max=\"10\" value=\"");
    sprintf(mod_bikes_nr, "%d", Setup->modes.mode[1].bikes);
    strcat(html, mod_bikes_nr);
    strcat(html, "\">\n"
                 " <div class=\"clear\"></div>\n"
                 " <hr>\n"
                 " <div class=\"txt\">");
    strcat(html, mod_mode);
    strcat(html, " 3</div>\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">&#8627;&nbsp;");
    strcat(html, mod_lap_mode);
    strcat(html, mod_lap_char);
    strcat(html, "</div>\n"
                 " <input type=\"radio\" name=\"mod_lap_gym_3\" value=\"lap\"");
    if (!Setup->modes.mode[2].lap_gym)
        strcat(html, " checked");
    strcat(html, ">\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">&#8627;&nbsp;");
    strcat(html, mod_gym_mode);
    strcat(html, mod_gym_char);
    strcat(html, "</div>\n"
                 " <input type=\"radio\" name=\"mod_lap_gym_3\" value=\"gym\"");
    if (Setup->modes.mode[2].lap_gym)
        strcat(html, " checked");
    strcat(html, ">\n"
                 " <div class=\"clear\"></div>\n"
                 " <div class=\"txt\">&nbsp;&nbsp;&#8627;&nbsp;");
    strcat(html, mod_bikes);
    strcat(html, "</div>\n"
                 " <input type=\"number\" name=\"mod_bikes_3\" min=\"1\" max=\"10\" value=\"");
    sprintf(mod_bikes_nr, "%d", Setup->modes.mode[2].bikes);
    strcat(html, mod_bikes_nr);
    strcat(html, "\">\n"
                 " <div class=\"clear\"></div>\n"
		 "<hr>\n"
                 " <button type=\"submit\">");
    strcat(html, mod_save);
    strcat(html, "</button>\n"
		 "</form>");

}

static void generate_csv(bool coma, char *html, int html_len, http_state* state){
              //           11111111112222222222333333333344
              // 012345678901234567890123456789012345678901
  char head[] = "Id;Lap/Gym;BykeId;Bykes;Time;Referee;Total\n";
  char sep[] = ";";
  if (coma){
    head[2] = ',';
    head[10] = ',';
    head[17] = ',';
    head[23] = ',';
    head[28] = ',';
    head[36] = ',';
    sep[0] = ',';
  }
  strcpy(html, head);
  for (int i = 0; i<readings.len; i++){
    if (readings.list[i].value != 0){
      append_id(html, readings.list[i].id); // ID
      strcat(html, sep);
      if(readings.list[i].lapGym == 0) // Lap/Gym
        strcat(html, "Lap");
      else
        strcat(html, "Gym");
      strcat(html, sep);
      append_id(html, readings.list[i].bykeNr); // BykeId
      strcat(html, sep);
      append_id(html, readings.list[i].bykes); // Bykes
      strcat(html, sep);

      append_val(html, readings.list[i].value); // Time
      strcat(html, sep);
      if (readings.list[i].referee > 0) // Referee
        append_val(html, readings.list[i].referee);
      if (readings.list[i].referee < 0)
        strcat(html, "bad run");
      strcat(html, sep);
      if (readings.list[i].referee > 0) // Referee
        append_val(html, readings.list[i].value + readings.list[i].referee);
      if (readings.list[i].referee < 0)
        strcat(html, "bad run");
      strcat(html, "\n");
    }
  }
}

static void generate_html(char *html, int html_len, http_state* state, char *error){
  if(state->page != enum_json)
    generate_html_head(html, html_len, state);
  switch(state->page){
    case enum_setup:
      LOG("generate setup body\n");
      generate_html_body_setup(html, html_len, state, error);
      break;
    case enum_modes:
      generate_html_body_modes(html, html_len, state, error);
      break;
    case enum_csv_col:
      generate_csv(separator_semicolon, html, html_len, state);
      return;
    case enum_csv_coma:
      generate_csv(separator_coma, html, html_len, state);
      return;
    case enum_json:
      strcat(html,REF_JSON);
      return;
    default:
      generate_html_body_main(html, html_len, state);
  }
  if(state->page != enum_json)
strcat(html,"<div class=\"clear\"></div>\n"
"</div>\n"
"</body></html>");
  LOG("generate DONE\n");
}

static void http_server_netconn_serve(struct netconn *conn, http_state *state)
{
  struct netbuf *inbuf;
  char *buf;
  u16_t buflen;
  err_t err;
  char *http_index_page = malloc(100000);
  if (http_index_page == NULL){
    LOG("Error: http_index_page malloc failed.\n");
    return;
  }
  http_index_page[0] = 0;
  char *error = malloc(100);
  if (error == NULL){
    LOG("Error: \"error\" malloc failed.\n");
    return;
  }
  error[0] = 0;
  char *http_user_agent;
  char *http_user_agent_end;
  char *ajax = malloc(5000);
  if (ajax == NULL){
    LOG("Error: ajax malloc failed.\n");
    return;
  }
  ajax[0] = 0;

  /* Read the data from the port, blocking if nothing yet there.
   We assume the request (the part we care about) is in one netbuf */
  err = netconn_recv(conn, &inbuf);

  if (err == ERR_OK) {
    netbuf_data(inbuf, (void**)&buf, &buflen);
    buf[buflen] = 0;

    /* Is this an HTTP GET command? (only check the first 5 chars, since
    there are other formats for GET, and we're keeping it very simple )*/
    if (buflen >= 5 && strncmp(buf, "GET /", 5) == 0){
      /* Send the HTML header
             * subtract 1 from the size, since we dont send the \0 in the string
             * NETCONN_NOCOPY: our data is const static, so no need to copy it
       */

      LOG("buf: <<<%s>>>\n", buf);

      if((strncmp(buf, "GET /clock", 10) != 0) && (strncmp(buf, "GET /ajax", 9) != 0)){
        LOG("User active\n");
        Setup->user_active = true;
      }
      if(strncmp(buf, "GET /ru", 7) == 0) {
        state->language = enum_ru;
      }
      else if(strncmp(buf, "GET /en", 7) == 0) {
        state->language = enum_en;
      }
      else if(strncmp(buf, "GET /lv", 7) == 0) {
        state->language = enum_lv;
      }
      else if(strncmp(buf, "GET /main", 9) == 0) {
        state->page = enum_main;
      }
      else if(strncmp(buf, "GET / ", 6) == 0) {
        state->page = enum_main;
      }
      else if(strncmp(buf, "GET /monitor", 12) == 0) {
        state->page = enum_monitor;
      }
      else if(strncmp(buf, "GET /referee", 12) == 0) {
        state->page = enum_referee;
      }
      else if(strncmp(buf, "GET /setup ", 11) == 0) {
        state->page = enum_setup;
      }
      else if(strncmp(buf, "GET /modes ", 11) == 0) {
        state->page = enum_modes;
      }
      else if((strncmp(buf, "GET /setup?", 11) == 0) || 
              (strncmp(buf, "GET /ap?", 8) == 0) ||
              (strncmp(buf, "GET /sta?", 9) == 0)) {
//GET /setup?type=ap&ip=192.168.0.1&gw=192.168.0.1&mask=255.255.255.0&ssid=MGR&password=12345&channel=5&authmode=open&con_limit=4&ip=0.0.0.0&gw=0.0.0.0&mask=0.0.0.0&ssid=&password=&bssid=0-0-0-0-0-0&dhcp=on&bssid=&bssid=&bssid= HTTP/1.1
        state->page = enum_setup;
        parse_setup(buf, state, error);
      }
      else if(strncmp(buf, "GET /modes?", 11) == 0) {
//GET /modes?mod_en_1=on&mod_lap_gym_1=lap&mod_bikes_1=4&mod_lap_gym_2=gym&mod_bikes_2=4&mod_bikes_3=4 HTTP/1.1
        state->page = enum_modes;
        parse_modes(buf, state, error);
      }
      else if(strncmp(buf, "GET /GetCol.csv", 15) == 0) {
        state->page = enum_csv_col;
      }
      else if(strncmp(buf, "GET /GetComa.csv", 16) == 0) {
        state->page = enum_csv_coma;
      }
      else if(strncmp(buf, "GET /jquery.min.js", 18) == 0) {
        state->page = enum_json;
        LOG("HTTP: json request\n");
      }// REF_JSON

      if(strncmp(buf, "GET /ap", 7) == 0) {
        Setup->wifi_conf.wifi_mode = false;
      }
      else if(strncmp(buf, "GET /sta", 8) == 0) {
        Setup->wifi_conf.wifi_mode = true;
      }

      LOG("HTTP: some mobile preprocessing\n");
      state->mobile = false;
      http_user_agent = strstr(buf, "User-Agent");
      http_user_agent_end = strstr(http_user_agent, "\n");
      for (int i = 0; i < MOBILE_AGENTS_NR; i++){
          char *m_agent = strstr(http_user_agent, mobile_agents[i]);
          if (m_agent != NULL && m_agent < http_user_agent_end){
              state->mobile = true;
              break;
          }
      }

      //LOG("Mobile agent found\n");

      if(state->page == enum_csv_col || state->page == enum_csv_coma)
        netconn_write(conn, http_csv_hdr, sizeof(http_csv_hdr)-1, NETCONN_NOCOPY);
      else
        netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);

      //LOG("Header ready\n");

      if(strncmp(buf, "GET /ajax", 9) == 0){
        /* Send out AJAX */
        LOG("AJAX: before processing\n");
        generate_ajax(ajax, readings.list, readings.len, state, readings.timerStep);
        netconn_write(conn, ajax, strlen(ajax), NETCONN_NOCOPY);
      }
      else if(strncmp(buf, "GET /clock", 9) == 0){
        /* Send out AJAX */
        LOG("Clock: before processing\n");
        strcat(ajax, "Time: ");
        strcat(ajax, Setup->ds_conf.date_time_ro);
        netconn_write(conn, ajax, strlen(ajax), NETCONN_NOCOPY);
      }
      else if(strncmp(buf, "GET /reset", 10) == 0){
        /* reset referee changes */
        int bid = 10;
        uint16_t id = 0;
        while (buf[bid] != ' '){
          id = id*10 + buf[bid] - '0';
          bid++;
        }
        for (int i = 0; i<readings.len; i++){
          if (id == readings.list[i].id){
            readings.list[i].referee = 0;
            break;
          }
        }
        LOG("GET: ResetRow %d\n", id);
      }
      else if(strncmp(buf, "GET /delete", 11) == 0){
        /* Strike out result */
        int bid = 11;
        uint16_t id = 0;
        while (buf[bid] != ' '){
          id = id*10 + buf[bid] - '0';
          bid++;
        }
        for (int i = 0; i<readings.len; i++){
          if (id == readings.list[i].id){
            readings.list[i].referee = -1;
            break;
          }
        }
        LOG("GET: DeleteRow %d\n", id);
      }
      else if(strncmp(buf, "GET /plus", 9) == 0){
        /* Strike out result */
        int bid = 9;
        uint16_t id = 0;
        while (buf[bid] != '_'){
          id = id*10 + buf[bid] - '0';
          bid++;
        }
        bid++;
        if (((buf[bid] >= '0') && (buf[bid] <= '9')) || (buf[bid] == ',') || (buf[bid] == '.')){
          bool goodVal = true;
          uint32_t val = 0;
          //process seconds first
          while ((buf[bid] != ' ') && (buf[bid] != ',') && (buf[bid] != '.')){
            if ((buf[bid] > '9') || (buf[bid] < '0'))
              goodVal = false;
            val = val*10 + (buf[bid] - '0') * 1000;
            LOG("GET: plus value %d to row %d bid is on '%c'\n", val, id, buf[bid]);
            bid++;
          }
          if(buf[bid] != ' '){
            bid++;
            int mul = 100;
            //process miliseconds first
            while ((buf[bid] != ' ') && (mul >= 1)){
              if ((buf[bid] > '9') || (buf[bid] < '0'))
                goodVal = false;
              val = val + (buf[bid] - '0') * mul;
              mul /= 10;
              LOG("GET: plus value %d to row %d bid is on '%c'\n", val, id, buf[bid]);
              bid++;
            }
          }
          if (goodVal){
            for (int i = 0; i<readings.len; i++){
              if (id == readings.list[i].id){
                readings.list[i].referee = val;
                break;
              }
            }
            LOG("GET: plus value %d to row %d\n", val, id);
          }
          else{
            LOG("GET: plus bad value detected");
	  }
        }
        else{
          LOG("GET: plus value not processed get '%c' on value start", buf[bid]);
	}
      }
      else if (strncmp(buf, "GET /favicon.ico", 16) == 0) {
        http_index_page[0] = 0;
        netconn_write(conn, http_index_page, strlen(http_index_page), NETCONN_NOCOPY);
      }
      else if (strncmp(buf, "GET /GetTime", 12) == 0) {
        generate_csv(separator_semicolon, http_index_page, strlen(http_index_page), state);
        netconn_write(conn, http_index_page, strlen(http_index_page), NETCONN_NOCOPY);
      }
      else if (strncmp(buf, "GET /GetStep", 12) == 0) {
        /* Send timer step */
        char* timeMeas = malloc(21);
        if (http_index_page == NULL){
          LOG("Error: GetStep timeMeas malloc failed.\n");
          return;
        }
        
        timeMeas[1] = (readings.timerStep % 10) + '0';
        timeMeas[0] = readings.timerStep / 10 + '0';
	timeMeas[2] = 0;

        if (readings.timerStep >= 0 && readings.timerStep <10){
          strcat(timeMeas, "=START ");
	  append_id(timeMeas, readings.timerStep + 1);
        }
        else if (readings.timerStep >= 10 && readings.timerStep <20){
          strcat(timeMeas, "=WAIT TO FINISH ");
	  append_id(timeMeas, readings.timerStep - 9);
        }

        LOG("GetStep: timer step=\"%d\"",readings.timerStep);
        netconn_write(conn, timeMeas, strlen(timeMeas), NETCONN_NOCOPY);
        free(timeMeas);
      }
      else {
        /* Send out HTML page */
        LOG("Generate html\n");
        generate_html(http_index_page, strlen(http_index_page), state, error);
	LOG("http_index_page size=%d\n", strlen(http_index_page));
        netconn_write(conn, http_index_page, strlen(http_index_page), NETCONN_NOCOPY);
      }
      //LOG("scan done\n");

      //gpio_set_level(LED_BUILTIN, 0);
    }
  }

  free(http_index_page);
  free(error);
  free(ajax);

  /* Close the connection (server closes in HTTP) */
  netconn_close(conn);

  /* Delete the buffer (netconn_recv gives us ownership,
   so we have to make sure to deallocate the buffer) */
  netbuf_delete(inbuf);
}

void http_server(void *pvParameters)
{
  struct netconn *conn, *newconn;
  http_state state;
  err_t err;
  Setup = pvParameters;
  conn = netconn_new(NETCONN_TCP);
  state.language = enum_ru;
  netconn_bind(conn, NULL, 80);
  netconn_listen(conn);
  do {
     err = netconn_accept(conn, &newconn);
     if (err == ERR_OK) {
         http_server_netconn_serve(newconn, &state);
         netconn_delete(newconn);
	 LOG("cycle done\n");
     }
  } while(err == ERR_OK);
  netconn_close(conn);
  netconn_delete(conn);
}

