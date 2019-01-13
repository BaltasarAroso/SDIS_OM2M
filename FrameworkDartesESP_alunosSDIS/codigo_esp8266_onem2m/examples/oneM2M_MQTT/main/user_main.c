#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "MQTTClient.h"

#include "om2m_mqtt.h"

#include "cJSON.h"



#define WIFI_SSID "xxx"
#define WIFI_PASS "xxx"
#define MQTT_BROKER "192.168.xxx.xxx"

#define BUF_SIZE                1000
#define TOTAL_CONSEQ_MSGS       25//100//25
#define DELAY_BTW_CONSEQ_MSGS   10000
#define N_HR_MEASUREMENTS       33//1//33//1//1//33

#define MQTT_CLIENT_THREAD_NAME         "mqtt_client_thread"
#define MQTT_CLIENT_THREAD_STACK_WORDS  6144
#define MQTT_CLIENT_THREAD_PRIO         1 //8

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

int counter_msg = 0;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    //WIFI_PS_MAX_MODEM WIFI_PS_NONE
    ESP_ERROR_CHECK( esp_wifi_set_ps(WIFI_PS_NONE) ); //sleep mode
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

/*void messageArrived1(MessageData* data) {

  printf("%d\n", counter_msg);
  counter_msg++;

}*/


void messageArrived1(MessageData* data) {
  printf("ola2\n");


  //unsigned char* message_str = (unsigned char*) data->message->payload;

  cJSON *message = NULL;
  cJSON *rqp = NULL;
  cJSON *pc = NULL;
  cJSON *pc_bad = NULL;
  cJSON *sgn = NULL;
  cJSON *nev = NULL;
  cJSON *rep = NULL;
  cJSON *cin = NULL;
  cJSON *con = NULL;

  printf("%d - ", counter_msg);
  counter_msg++;

  //message_str[data->message->payloadlen] = '\0';
//  printf("Message arrived: %s\n", data->message->payload);


  if(data->message->payloadlen > 800) {


    //printf("Message arrived: %c\n", &data->message->payload+10);
  //  printf("Size: %d\n", data->message->payloadlen);

    //free(message_str);



    message = cJSON_Parse(data->message->payload);

    rqp = cJSON_GetObjectItem(message,"m2m:rqp");
    pc_bad = cJSON_GetObjectItem(rqp,"m2m:pc");

    char* pc_str = cJSON_PrintUnformatted(pc_bad);
    char* pc_str_json = malloc(sizeof(char*) * strlen(pc_str));

    if(pc_str_json == NULL) {
      printf("Erro de memória\n");
      return;
    }

    int i = 0, j = 0;

    for(i = 1; i < strlen(pc_str) - 1; i++) {
      if(pc_str[i] == ' ')
        continue;
      else if (pc_str[i] == '\\' && pc_str[i+1] == 'n') {
        i++;
        continue;
      }
      else if (pc_str[i] == '\\') {
        continue;
      }
      else {
        pc_str_json[j] = pc_str[i];
        j++;
      }
    }
    pc_str_json[j] = '\0';


    //printf("string-%s\n", pc_str_json);
    pc = cJSON_Parse(pc_str_json);

    sgn = cJSON_GetObjectItem(pc,"m2m:sgn");
    nev = cJSON_GetObjectItem(sgn,"m2m:nev");
    rep = cJSON_GetObjectItem(nev,"m2m:rep");
    cin = cJSON_GetObjectItem(rep,"m2m:cin");
    con = cJSON_GetObjectItem(cin,"con");


   printf("%s\n", con->valuestring);

    //printf("Message arrived2: %s\n", cJSON_PrintUnformatted(cin));
  //  printf("array size  = %d\n", cJSON_GetArraySize(cin));

//    printf("Size: %d\n", data->message->payloadlen);


    free(pc_str_json);
    free(pc_str);
    cJSON_Delete(pc);
    cJSON_Delete(sgn);
    cJSON_Delete(nev);
    cJSON_Delete(rep);
    cJSON_Delete(cin);
    cJSON_Delete(con);
    cJSON_Delete(message);
    cJSON_Delete(rqp);
    cJSON_Delete(pc_bad);
  }
}



void mqtt_client_thread(void *pvParameters) {
  xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                  false, true, portMAX_DELAY);  //wait for AP connection

  unsigned char *sendbuf = (unsigned char*) malloc(sizeof(unsigned char) * MQTT_SEND_BUF_SIZE);
  if(sendbuf == NULL)
    ESP_LOGE(TAG, "Memory allocation error -> sendbuf");

  unsigned char *readbuf = (unsigned char*) malloc(sizeof(unsigned char) * MQTT_READ_BUF_SIZE);
  if(readbuf == NULL)
    ESP_LOGE(TAG, "Memory allocation error -> readbuf");

  MQTTClient client;
  Network network;
  NetworkInit(&network);
  MQTTClientInit(&client, &network, MQTT_TIMEOUT, sendbuf, MQTT_SEND_BUF_SIZE, readbuf, MQTT_READ_BUF_SIZE);

  MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
  tcpip_adapter_ip_info_t local_ip;

  char clientID[30];
  //sprintf(clientID, "ESP8266_"IPSTR"", IP2STR(&local_ip.ip));
  sprintf(clientID, "ESP8266_192.168.1.106");

  connectData.MQTTVersion = 4;
  connectData.clientID.cstring = clientID;
  connectData.cleansession = 0;

  if( NetworkConnect(&network, MQTT_BROKER_IP, MQTT_BROKER_PORT) != 0 )
    ESP_LOGE(TAG, "NetworkConnect error");

  if( MQTTStartTask(&client) != pdPASS)
    ESP_LOGE(TAG, "MQTTStartTask error");

  if ( MQTTConnect(&client, &connectData) != 0 )
    ESP_LOGE(TAG, "MQTTConnect error");

  char sub_topic[50];
  sprintf(sub_topic, "/oneM2M/req/%s/json", clientID);
  if ( MQTTSubscribe(&client, sub_topic, 1, messageArrived1) != 0 )
    ESP_LOGE(TAG, "MQTTSubscribe error");
  else
    ESP_LOGI(TAG, "MQTT subscribe to topic %s", sub_topic);

  om2m_mqtt_create_ae(&client, "esp_sub_mqtt", 67898);
  //om2m_create_subscription(&client, "esp_pub_mqtt", "HR", "esp_sub_mqtt", "sub_mqtt");
  om2m_create_subscription(&client, "pc_pub", "HR", "esp_sub_mqtt", "sub_mqtt");


  while (1) {
    //printf("%d\n", period);
    vTaskDelay(10000/portTICK_RATE_MS);
  }


  om2m_mqtt_create_ae(&client, "esp_pub", 67892);
  om2m_mqtt_create_container(&client, "esp_pub", "HR");



  char con[85] = {"73hdjetyru4682jeir638rnforte63uwir6390kmgsi2538endi84jdo7svcndghlduyduedsr353wefds43s"};
  char rn[10];
  int i = 0;

  vTaskDelay(5000/portTICK_RATE_MS);

  while(1) {
    sprintf(rn, "%07d", i);
    om2m_mqtt_create_content_instance(&client, "esp_pub", "HR", rn, con);

    if(i==100)
      break;

    vTaskDelay(10000/portTICK_RATE_MS);
    i++;
  }

  while (1) {
    //printf("%d\n", period);
    vTaskDelay(10000/portTICK_RATE_MS);
  }
}


void app_main(void)
{
  ESP_ERROR_CHECK( nvs_flash_init() );
  initialise_wifi();
  xTaskCreate(&mqtt_client_thread,
              MQTT_CLIENT_THREAD_NAME,
              MQTT_CLIENT_THREAD_STACK_WORDS,
              NULL,
              MQTT_CLIENT_THREAD_PRIO,
              NULL);
}
