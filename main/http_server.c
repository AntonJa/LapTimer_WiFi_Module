#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "freertos/FreeRTOS.h"
#include "string.h"
#include "fn_defs.h"
#include "structures.h"
#include "http_server.h"
#include "lwip/api.h"
#include "driver/gpio.h"
#include "constants.h"

const static char http_html_hdr[] =
    "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";

const static char http_csv_hdr[] =
    "HTTP/1.1 200 OK\r\nContent-type: csv\r\n\r\n";

enum enum_Lang{
  enum_ru=0,
  enum_lv=1,
  enum_en=2
};

enum enum_Page{
  enum_main=0,
  enum_monitor=1,
  enum_setup=2,
  enum_csv_col=3,
  enum_csv_coma=4,
  enum_referee=5,
  enum_json=6
};

enum enum_timerStep{
  enum_timer_start=0,
  enum_timer_beam_on=1,
  enum_timer_run=2,
  enum_timer_end=3,
  enum_timer_reset=4
};

#define setup_save_str_NR 22

#define separator_coma true
#define separator_semicolon false

enum enum_setup_save{
  enum_setup_ap_ip=1,
  enum_setup_ap_gw=2,
  enum_setup_ap_mask=3,
  enum_setup_ap_ssid=4,
  enum_setup_ap_password=5,
  enum_setup_ap_channel=6,
  enum_setup_ap_authmode=7,
  enum_setup_ap_con_limit=8,
  enum_setup_ap_hide_ssid=9,
  enum_setup_sta_ssid=10,
  enum_setup_sta_password=11,
  enum_setup_sta_bssid=12,
  enum_setup_sta_use_mac=13,
  enum_setup_ds_seconds=14,
  enum_setup_ds_minutes=15,
  enum_setup_ds_hours=16,
  enum_setup_ds_date=17,
  enum_setup_ds_month=18,
  enum_setup_ds_year=19,
  enum_setup_ds_aging=20
};

const static char *setup_save_str[] = 
    {"ap_ip",
    "ap_gw",
    "ap_mask",
    "ap_ssid",
    "ap_password",
    "ap_channel",
    "ap_authmode",
    "ap_con_limit",
    "ap_hide_ssid",
    "sta_ssid",
    "sta_password",
    "sta_bssid",
    "sta_use_mac",
    "ds_sec",
    "ds_min",
    "ds_hur",
    "ds_dat",
    "ds_mon",
    "ds_yer",
    "ds_age"};

typedef struct {
   enum enum_Lang language;
   enum enum_Page page;
   bool mobile;
} http_state;

setup_type *Setup;

#define MOBILE_AGENTS_NR 13

const static char *mobile_agents[] = 
    {"Android",
    "webOS",
    "iPhone",
    "iPad",
    "iPod",
    "BlackBerry",
    "BB",
    "PlayBook",
    "IEMobile",
    "Windows Phone",
    "Kindle",
    "Silk",
    "Opera Mini"};
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
void generate_ajax(char *ajax, reading_type* list,int len, http_state *state,int8_t timerStep){
    printf("AJAX: start processing\n");
    if (list == NULL){
        strcpy(ajax, "\n");
        return;
    }
    strcpy(ajax, "<table>\n");
    printf("AJAX: selecting language %d\n", state->language);
    switch(state->language){
        case enum_ru:
            strcat(ajax, " <tr>\n"
                         "  <th name=\"run\">Заезд</th>\n"
                         "  <th name=\"time\">Время</th>\n"
                         " </tr>\n");
            if (timerStep >= enum_timer_start && timerStep <= enum_timer_beam_on){
              strcat(ajax, "  <tr>\n"
                           "    <td colspan=\"2\">СTАРТ</td>\n");
              strcat(ajax, "  </tr>\n");
            }
            if (timerStep == enum_timer_run){
              strcat(ajax, "  <tr>\n"
                           "    <td colspan=\"2\">ЕДЕТ</td>\n");
              strcat(ajax, "  </tr>\n");
            }
            break;
        case enum_lv:
            strcat(ajax, " <tr>\n"
                         "  <th name=\"run\">Brauc.</th>\n"
                         "  <th name=\"time\">Laiks</th>\n"
                         " </tr>\n");
            if (timerStep >= enum_timer_start && timerStep <= enum_timer_beam_on){
              strcat(ajax, "  <tr>\n"
                           "    <td colspan=\"2\">STARTS</td>\n"
                           "  </tr>\n");
            }
            if (timerStep == enum_timer_run){
              strcat(ajax, "  <tr>\n"
                           "    <td colspan=\"2\">BRAUCIENS</td>\n"
                           "  </tr>\n");
            }
            break;
        case enum_en:
            strcat(ajax, " <tr>\n"
                         "  <th name=\"run\">Run</th>\n"
                         "  <th name=\"time\">Time</th>\n"
                         " </tr>\n");
            if (timerStep >= enum_timer_start && timerStep <= enum_timer_beam_on){
              strcat(ajax, "  <tr>\n"
                           "    <td colspan=\"2\">START</td>\n"
                           "  </tr>\n");
            }
            if (timerStep == enum_timer_run){
              strcat(ajax, "  <tr>\n"
                           "    <td colspan=\"2\">RUNNING</td>\n"
                           "  </tr>\n");
            }
            break;
    }
    //populate list
    printf("AJAX: populating list\n");
    int max = len;
    if (len > 30) max = 30;
    for (int i = 0; i<max; i++){
        if (list[i].value != 0){
            strcat(ajax, "  <tr>\n"
              "    <td>");
            append_id(ajax, list[i].id);
            strcat(ajax, "</td>\n"
              "    <td>");
            if (list[i].referee < 0)
              strcat(ajax, "<s>");
            append_val(ajax, list[i].value);
            if (list[i].referee > 0){
              strcat(ajax, "(+");
              append_val(ajax, list[i].referee);
              strcat(ajax, ")");
            }
            if (list[i].referee < 0)
              strcat(ajax, "</s>");
            strcat(ajax, "</td>\n"
              "  </tr>\n");
        }
    }
    strcat(ajax, "</table> ");
    printf("AJAX: ready\n");
}

static void generate_html_head(char *html, int html_len, http_state* state){
    char *P0 = malloc(35);//[35];
    if (P0 == NULL){
      printf("Error: P0 malloc failed.\n");
      return;
    }
    P0[0] = 0;
    char *P1 = malloc(35);//[35];
    if (P1 == NULL){
      printf("Error: P1 malloc failed.\n");
      free(P0);
      return;
    }
    P1[0] = 0;
    char *P2 = malloc(35);//[35];
    if (P2 == NULL){
      printf("Error: P2 malloc failed.\n");
      free(P0);
      free(P1);
      return;
    }
    P2[0] = 0;
    char *P3 = malloc(35);//[35];
    if (P3 == NULL){
      printf("Error: P3 malloc failed.\n");
      free(P0);
      free(P1);
      free(P2);
      return;
    }
    P3[0] = 0;
    char *P31 = malloc(95);//[95];
    if (P31 == NULL){
      printf("Error: P31 malloc failed.\n");
      free(P0);
      free(P1);
      free(P2);
      free(P3);
      return;
    }
    P31[0] = 0;
    char *P41 = malloc(95);//[95];
    if (P41 == NULL){
      printf("Error: P41 malloc failed.\n");
      free(P0);
      free(P1);
      free(P2);
      free(P3);
      free(P31);
      return;
    }
    P41[0] = 0;
    char *P5 = malloc(35);//[35];
    if (P5 == NULL){
      printf("Error: P5 malloc failed.\n");
      free(P0);
      free(P1);
      free(P2);
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
        mTableF[0] = '6';
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
    if((Setup->wifi_conf.save >= 1) && (Setup->wifi_conf.save <= 5) && state->page == enum_setup){
        strcat(html, " if(rlds>0)window.location = \"setup\";\n");
    }
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
    char ds_savi[24] = {0};
    switch(state->language){
        case enum_lv:     //012345678901234
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
    if((Setup->wifi_conf.save >= 1) && (Setup->wifi_conf.save <= 5))
        strcat(html, ds_savi);
    else
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

static void generate_csv(bool coma, char *html, int html_len, http_state* state){
  char head[] = "Run;Time\n";
  char sep[] = ";";
  if (coma){
    head[3] = ',';
    sep[0] = ',';
  }
  strcpy(html, head);
  for (int i = 0; i<readings.len; i++){
    if (readings.list[i].value != 0){
      append_id(html, readings.list[i].id);
      strcat(html, sep);

      append_val(html, readings.list[i].value);
      strcat(html, sep);
      if (readings.list[i].referee > 0){
        append_val(html, readings.list[i].referee);
      }
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
      generate_html_body_setup(html, html_len, state, error);
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
}

static void decode_ip(uint8_t* ip, char* buf){
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

static void decode_mac(uint8_t* ip, char* buf){//00:1D:73:55:2D:1D
  int i[6] = {0};
  int id = 0;
  while(id<6 && ((buf[0] >= '0' && buf[0] <= '9') || 
                 (buf[0] >= 'a' && buf[0] <= 'f') || 
                 (buf[0] >= 'A' && buf[0] <= 'F') || 
                 (buf[0]=='%' && buf[1]=='3' && buf[2]=='A'))){

    if (buf[0]=='%' && buf[1]=='3' && buf[2]=='A')
        buf += 3;

    printf("MAC readed %c%c\n", buf[0], buf[1]);
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
      printf("%c->%d\n", buf[0], i[id]);
      buf++;
      if (i[id] > 255)
        return;
    }
    printf(" %d\n", i[id]);
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
    printf("%d\n", ret);
  }
  return ret;
}

void parse_setup(char* buf, http_state *state, char *error){
  bool save_wifi_conf = false;
  error[0] = 0;
  while(buf[0]!='?')
    buf++;
  buf++;//type=ap&ip=192.168.0.1&gw=192.168.0.1&mask=255.255.255.0&ssid=MGR&password=12345&channel=5&authmode=open&con_limit=4&ip=0.0.0.0&gw=0.0.0.0&mask=0.0.0.0&ssid=&password=&bssid=0-0-0-0-0-0&dhcp=on&bssid=&bssid=&bssid= HTTP/1.1
  
  while(buf[0] != 0 && buf[0] != ' '){
    printf("start\n");
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
           printf("MAC filter ON found\n");
           save_wifi_conf = true;
           break;
      case enum_setup_ds_seconds://ds_sec=
           printf("HTTP: seconds start\n");
           Setup->ds_conf.www_busy = true;
           while(Setup->ds_conf.i2c_busy){
               // check if both was set at one time
               if(Setup->ds_conf.i2c_waiting){
                   break;
               }
           }
           Setup->ds_conf.bcd_seconds = s2bcd(buf);
           printf("seconds = %02x\n", Setup->ds_conf.bcd_seconds);
           break;
      case enum_setup_ds_minutes://ds_min=
           Setup->ds_conf.bcd_minutes = s2bcd(buf);
           printf("minutes = %02x\n", Setup->ds_conf.bcd_minutes);
           break;
      case enum_setup_ds_hours://ds_hur=
           Setup->ds_conf.bcd_hour = s2bcd(buf);
           printf("hours = %02x\n", Setup->ds_conf.bcd_hour);
           break;
      case enum_setup_ds_date://ds_dat=
           Setup->ds_conf.bcd_date = s2bcd(buf);
           printf("date = %02x\n", Setup->ds_conf.bcd_date);
           break;
      case enum_setup_ds_month://ds_mon=
           Setup->ds_conf.bcd_month = s2bcd(buf);
           printf("month = %02x\n", Setup->ds_conf.bcd_month);
           break;
      case enum_setup_ds_year://ds_yer=
           year = s2bcd(buf);
           if(year > 0x2099)
               Setup->ds_conf.century = true;
           else
               Setup->ds_conf.century = false;
           Setup->ds_conf.bcd_year = year & 0xff;
           printf("year = %02x, century = %d\n", Setup->ds_conf.bcd_year, Setup->ds_conf.century);
           break;
      case enum_setup_ds_aging://ds_age=
           Setup->ds_conf.aging = s2i(buf);
           printf("agning = %d\n", Setup->ds_conf.aging);
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
    printf("end <%c>\n", buf[0]);
  }
  if(save_wifi_conf)
    Setup->wifi_conf.save = 1;
  printf("HTML: time end changed=%d\n", Setup->ds_conf.changed);
  Setup->ds_conf.www_busy = false;
}

static void http_server_netconn_serve(struct netconn *conn, http_state *state)
{
  struct netbuf *inbuf;
  char *buf;
  u16_t buflen;
  err_t err;
  char *http_index_page = malloc(100000);
  if (http_index_page == NULL){
    printf("Error: http_index_page malloc failed.\n");
    return;
  }
  http_index_page[0] = 0;
  char *error = malloc(100);
  if (error == NULL){
    printf("Error: \"error\" malloc failed.\n");
    return;
  }
  error[0] = 0;
  char *http_user_agent;
  char *http_user_agent_end;
  char *ajax = malloc(5000);
  if (ajax == NULL){
    printf("Error: ajax malloc failed.\n");
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

      printf("buf: <<<%s>>>\n", buf);

      if((strncmp(buf, "GET /ajax", 9) != 0) && (Setup->wifi_conf.save == 0)){
        Setup->wifi_conf.save = 6;
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
      else if((strncmp(buf, "GET /setup?", 11) == 0) || 
              (strncmp(buf, "GET /ap?", 8) == 0) ||
              (strncmp(buf, "GET /sta?", 9) == 0)) {
//GET /setup?type=ap&ip=192.168.0.1&gw=192.168.0.1&mask=255.255.255.0&ssid=MGR&password=12345&channel=5&authmode=open&con_limit=4&ip=0.0.0.0&gw=0.0.0.0&mask=0.0.0.0&ssid=&password=&bssid=0-0-0-0-0-0&dhcp=on&bssid=&bssid=&bssid= HTTP/1.1
        state->page = enum_setup;
        parse_setup(buf, state, error);
      }
      else if(strncmp(buf, "GET /GetCol.csv", 15) == 0) {
        state->page = enum_csv_col;
      }
      else if(strncmp(buf, "GET /GetComa.csv", 16) == 0) {
        state->page = enum_csv_coma;
      }
      else if(strncmp(buf, "GET /jquery.min.js", 18) == 0) {
        state->page = enum_json;
        printf("HTTP: json request\n");
      }// REF_JSON

      if(strncmp(buf, "GET /ap", 7) == 0) {
        Setup->wifi_conf.wifi_mode = false;
      }
      else if(strncmp(buf, "GET /sta", 8) == 0) {
        Setup->wifi_conf.wifi_mode = true;
      }

      printf("HTTP: some mobile preprocessing\n");
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

      //printf("HTML: Mobile agent found\n");

      if(state->page == enum_csv_col || state->page == enum_csv_coma)
        netconn_write(conn, http_csv_hdr, sizeof(http_csv_hdr)-1, NETCONN_NOCOPY);
      else
        netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);

      //printf("HTML: Header ready\n");

      if(strncmp(buf, "GET /ajax", 9) == 0){
        /* Send out AJAX */
        printf("AJAX: before processing\n");
        generate_ajax(ajax, readings.list, readings.len, state, readings.timerStep);
        netconn_write(conn, ajax, strlen(ajax), NETCONN_NOCOPY);
      }
      else if(strncmp(buf, "GET /clock", 9) == 0){
        /* Send out AJAX */
        printf("Clock: before processing\n");
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
        printf("GET: ResetRow %d\n", id);
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
        printf("GET: DeleteRow %d\n", id);
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
            printf("GET: plus value %d to row %d bid is on '%c'\n", val, id, buf[bid]);
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
              printf("GET: plus value %d to row %d bid is on '%c'\n", val, id, buf[bid]);
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
            printf("GET: plus value %d to row %d\n", val, id);
          }
          else
            printf("GET: plus bad value detected");
        }
        else
          printf("GET: plus value not processed get '%c' on value start", buf[bid]);
      }
      else if (strncmp(buf, "GET /GetTime", 12) == 0) {
        /* Send latest measured time */
        /*char* timeMeas = malloc(100);
        if (http_index_page == NULL){
          printf("Error: GetTime timeMeas malloc failed.\n");
          return;
        }

        timeMeas[0] = 0;
        if (readings.list[0].value != 0){
          append_id(timeMeas, readings.list[0].id);
          strcat(timeMeas, "=");
          append_val(timeMeas, readings.list[0].value);

          append_val(timeMeas, readings.list[0].value);
          if (readings.list[0].referee > 0){
            strcat(timeMeas, "(+");
            append_val(timeMeas, readings.list[0].referee);
            strcat(timeMeas, ")");
          }
          if (readings.list[0].referee < 0)
            strcat(timeMeas, "(bad run)");

        }
        else {
          strcat(timeMeas, "null");
        }
        printf("GetTime: timeMeas=\"%s\"\n",timeMeas);
        netconn_write(conn, timeMeas, strlen(timeMeas), NETCONN_NOCOPY);
        free(timeMeas);*/
        generate_csv(separator_semicolon, http_index_page, strlen(http_index_page), state);
        netconn_write(conn, http_index_page, strlen(http_index_page), NETCONN_NOCOPY);
      }
      else if (strncmp(buf, "GET /GetStep", 12) == 0) {
        /* Send timer step */
        char* timeMeas = malloc(10);
        if (http_index_page == NULL){
          printf("Error: GetStep timeMeas malloc failed.\n");
          return;
        }
        
        timeMeas[0] = (readings.timerStep % 10) + '0';
        timeMeas[1] = 0;

        if (readings.timerStep == 0){
          strcat(timeMeas, "=START");
        }
        else if (readings.timerStep == 1){
          strcat(timeMeas, "=BEAM ON START");
        }
        else if (readings.timerStep == 2){
          strcat(timeMeas, "=RUNNING");
        }
        else if (readings.timerStep == 3){
          strcat(timeMeas, "=FINISH");
        }
        else if (readings.timerStep == 4){
          strcat(timeMeas, "=BEAM ON RESET");
        }

        printf("GetStep: timer step=\"%d\"",readings.timerStep);
        netconn_write(conn, timeMeas, strlen(timeMeas), NETCONN_NOCOPY);
        free(timeMeas);
      }
      else {
        /* Send out HTML page */
        printf("HTML: Generate html\n");
        generate_html(http_index_page, strlen(http_index_page), state, error);
        netconn_write(conn, http_index_page, strlen(http_index_page), NETCONN_NOCOPY);
      }
      //printf("HTML: scan done\n");

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
     }
  } while(err == ERR_OK);
  netconn_close(conn);
  netconn_delete(conn);
}

