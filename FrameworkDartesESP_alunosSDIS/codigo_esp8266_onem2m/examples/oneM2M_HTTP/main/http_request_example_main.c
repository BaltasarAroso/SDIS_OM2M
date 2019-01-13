/* HTTP GET Example using plain POSIX sockets

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_sleep.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "malloc.h"
#include "cJSON.h"

#include "coap.h"

#include "nvs_flash.h"

#include <netdb.h>
#include <sys/socket.h>


/*#define WIFI_SSID "jtmesquita"
#define WIFI_PASS "jtmesquita"*/
#define WIFI_SSID "xxx"
#define WIFI_PASS "xxx"



#define HTTP_SERVER_PORT 1800

int period = 1;


/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

/* Constants that aren't configurable in menuconfig */
//#define CSE_IP "192.168.106.128"
//#define CSE_IP "192.168.106.129"
#define CSE_IP "192.168.106.129"
#define CSE_PORT 8080
#define CSE_NAME "dartes"

static const char *TAG = "example";

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
            .listen_interval = 100,
        },
    };
    // WIFI_PS_MAX_MODEM
    // WIFI_PS_NONE
    ESP_ERROR_CHECK( esp_wifi_set_ps(WIFI_PS_MAX_MODEM) );
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static int establish_connection_server(char *ip, int port) {
    int clientfd;
    struct sockaddr_in serveraddr;

    bzero(&serveraddr,	sizeof(struct	sockaddr_in));

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                    false, true, portMAX_DELAY); //wait for AP connection

    serveraddr.sin_family	=	AF_INET;
    serveraddr.sin_port	=	htons(port);
    inet_aton(ip, &serveraddr.sin_addr.s_addr);

    clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(clientfd < 0) {
      ESP_LOGE(TAG, "... Failed to allocate socket.");
      close(clientfd);
      return -1;
    }
    ESP_LOGI(TAG, "... allocated socket");

    if ( connect(clientfd, &serveraddr, sizeof(serveraddr)) < 0 ) {
      ESP_LOGE(TAG, "... socket connect failed");
      close(clientfd);
      return -1;
    }
    return clientfd;
}

static int create_localserver_socket() {
    int serverfd;
    struct sockaddr_in http_serveraddr;

    bzero(&http_serveraddr,	sizeof(struct	sockaddr_in));

    http_serveraddr.sin_family	=	AF_INET;
    http_serveraddr.sin_port	=	htons(HTTP_SERVER_PORT);
    http_serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                    false, true, portMAX_DELAY); //wait for AP connection

    if ( (serverfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
      ESP_LOGE(TAG, "... Failed to allocate socket.");
      close(serverfd);
      return -1;
    }

    if ( bind(serverfd, (struct sockaddr *) &http_serveraddr, sizeof(http_serveraddr)) < 0 ) {
      ESP_LOGE(TAG, "... Failed to bind socket.");
      close(serverfd);
      return -1;
    }

    if ( listen(serverfd, 5) < 0 ) {
      ESP_LOGE(TAG, "... Failed to listen on socket.");
      close(serverfd);
      return -1;
    }

  /*struct ip_info info;
    wifi_get_ip_info(STATION_IF, &info);
    ESP_LOGI("HTTP Server listening on %s:%d\n", ipaddr_ntoa(&info.ip), HTTP_SERVER_PORT);*/

    return serverfd;
}



static void new_client_task(void *pvParameters) {
  char *buf = malloc(sizeof(char)*2000);
  memset(buf, 0, 2000);
  int clientfd = *(int*)pvParameters;

  int read = recv(clientfd, buf, 2000, 0);
  om2m_http_ok(clientfd);

  char *payload = NULL;
  payload = strstr(buf, "{");

  if(strlen(payload) > 300) {
    cJSON *payload_json = NULL;
    cJSON *sgn = NULL;
    cJSON *nev = NULL;
    cJSON *rep = NULL;
    cJSON *cin = NULL;
    cJSON *rn = NULL;
    cJSON *con = NULL;

    payload_json = cJSON_Parse(payload);
    sgn = cJSON_GetObjectItem(payload_json, "m2m:sgn");
    nev = cJSON_GetObjectItem(sgn, "m2m:nev");
    rep = cJSON_GetObjectItem(nev, "m2m:rep");
    cin = cJSON_GetObjectItem(rep, "m2m:cin");
    rn = cJSON_GetObjectItem(cin, "rn");
    con = cJSON_GetObjectItem(cin, "con");

    printf("%s + %s\n", rn->valuestring, con->valuestring);

    period = atoi(rn->valuestring);

    //printf("%s\n", cJSON_PrintUnformatted(payload_json));

    cJSON_Delete(payload_json);
    cJSON_Delete(sgn);
    cJSON_Delete(nev);
    cJSON_Delete(rep);
    cJSON_Delete(cin);
    cJSON_Delete(rn);
    cJSON_Delete(con);
  }
  free(buf);
  close(clientfd);
  vTaskDelete(NULL);
}

static void server_task(void *arg) {

  struct sockaddr_in http_clientaddr;
  socklen_t addr_len;

  int serverfd = create_localserver_socket();
  int clientfd = -1;
  //void *pointer;

  printf("serverfd = %d\n", serverfd);

  while (1) {
    int client_len = sizeof(http_clientaddr);
    printf("Waiting...\n");
    clientfd = accept(serverfd, (struct sockaddr*)&http_clientaddr, &addr_len);
    printf("aceiteeieiii\n");

    if (clientfd < 0) {
      printf("Could not establish new connection\n");
      continue;
    }

    printf("clientfd1 = %d\n", clientfd);

    xTaskCreate(new_client_task, "new_client_task", 2048, (void*)&clientfd, 2, NULL);
  }
}

static void client_task(void *arg) {
  int clientfd = -1;
  while( (clientfd = establish_connection_server(CSE_IP, CSE_PORT)) < 0 ) {
    printf("Server connection error! Retrying in 1s...\n");
    vTaskDelay(1000/portTICK_RATE_MS);
  }

  printf("Server connection with sucess!\n");

  om2m_create_ae(clientfd, "esp_sub_http", 67898);
  //om2m_create_subscription(&client, "esp_pub_mqtt", "HR", "esp_sub_mqtt", "sub_mqtt");
  om2m_create_subscription(clientfd, "pc_pub", "HR", "esp_sub_http", "sub_http");

  /*om2m_create_ae(clientfd, "esp_sub_http", 67811);
  om2m_create_subscription(clientfd, "esp_pub_http", "HR", "esp_sub_http", "sub_http");
  om2m_create_subscription(clientfd, "esp_pub_mqtt", "HR", "esp_sub_http", "sub_http");*/



  //om2m_http_delete(clientfd, "/~/in-cse/dartes/master");
  //om2m_http_delete(clientfd, "/~/in-cse/dartes/monitor");

  //om2m_create_ae(clientfd, "master", 67891);
  //om2m_create_container(clientfd, "master", "teste");

//  vTaskDelay(5000/portTICK_RATE_MS);

/*  char teste[5];
  sprintf(teste,"%s", "5");
  om2m_create_content_instance(clientfd, "master", "teste", teste, "olá mundo!");*/

  /*om2m_create_ae(clientfd, "esp_pub_http", 67876);
  om2m_create_container(clientfd, "esp_pub_http", "HR");

  int i = 1;
  char rn[10];
  char con[85] = {"73hdjetyru4682jeir638rnforte63uwir6390kmgsi2538endi84jdo7svcndghlduyduedsr353wefds43s"};
  //char con[850] = {"73hdjetyru4682jeir638rnforte63uwir6390kmgsi2538endi84jdo7svcndghlduyduedsr353wefds43s73hdjetyru4682jeir638rnforte63uwir6390kmgsi2538endi84jdo7svcndghlduyduedsr353wefds43s73hdjetyru4682jeir638rnforte63uwir6390kmgsi2538endi84jdo7svcndghlduyduedsr353wefds43s73hdjetyru4682jeir638rnforte63uwir6390kmgsi2538endi84jdo7svcndghlduyduedsr353wefds43s73hdjetyru4682jeir638rnforte63uwir6390kmgsi2538endi84jdo7svcndghlduyduedsr353wefds43s73hdjetyru4682jeir638rnforte63uwir6390kmgsi2538endi84jdo7svcndghlduyduedsr353wefds43s73hdjetyru4682jeir638rnforte63uwir6390kmgsi2538endi84jdo7svcndghlduyduedsr353wefds43s73hdjetyru4682jeir638rnforte63uwir6390kmgsi2538endi84jdo7svcndghlduyduedsr353wefds43s73hdjetyru4682jeir638rnforte63uwir6390kmgsi2538endi84jdo7svcndghlduyduedsr353wefds43s73hdjetyru4682jeir638rnforte63uwir6390kmgsi2538endi84jdo7svcndghlduyduedsr353wefds43s"};
  char *buf = malloc(sizeof(char)*2000);

//  vTaskDelay(5000/portTICK_RATE_MS);

  while(1) {
    int time1 = 56789;
    sprintf(rn, "%07d", i);
    om2m_create_content_instance(clientfd, "esp_pub_http", "HR", rn, con);

    memset(buf, 0, 2000);
    int read = recv(clientfd, buf, 2000, 0);

    vTaskDelay(1000/portTICK_RATE_MS);
    i++;
  }
  //om2m_create_ae(clientfd, "monitor", 67892);
  //om2m_create_subscription(clientfd, "esp_pub", "HR", "monitor", "sub");
  /*char teste[] = {"{\"m2m:ae\":{\"rr\":false,\"api\":67891,\"rn\":\"master\"}}"};
  printf("om2m_http_post = %d\n", om2m_http_post(clientfd, CSE_IP, CSE_PORT, "/~/in-cse/dartes", teste));
  recv(clientfd, buf, 300, 0);
  printf("%s", buf);
  memset(buf, 0, 300);*/

while (1) {
    //printf("%d\n", period);
    vTaskDelay(10000/portTICK_RATE_MS);
  }
}


void app_main()
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    initialise_wifi();

    xTaskCreate(&client_task, "client_task", 16384, NULL, 5, NULL);
    xTaskCreate(&server_task, "server_task", 16384, NULL, 5, NULL);
}
