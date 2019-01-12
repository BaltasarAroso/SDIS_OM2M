#include <netdb.h>
#include <string.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "nvs_flash.h"

#include <om2m/coap.h>

#include "max30100.h"
#include "om2m_coap_config.h"
#include "cJSON.h"

#define COAP_SERVER_PORT 5683
#define TESTE_PUBLISH
#define TESTE_SUB_PUB
#define DEBUG_COAP
#define TESTING

typedef struct
{
  char *entity;
  char *container;
} uri;

unsigned int wait_seconds = 90; /* default timeout in seconds */
coap_tick_t max_wait;           /* global timeout (changed by set_timeout()) */

unsigned int obs_seconds = 30; /* default observe time */
coap_tick_t obs_wait = 0;      /* timeout for current subscription */

#define min(a, b) ((a) < (b) ? (a) : (b))

static inline void set_timeout(coap_tick_t *timer, const unsigned int seconds)
{
  coap_ticks(timer);
  *timer += seconds * COAP_TICKS_PER_SECOND;
}

uint16_t ir_buffer[MAX30100_FIFO_DEPTH];
uint16_t red_buffer[MAX30100_FIFO_DEPTH];
size_t data_len;

static EventGroupHandle_t coap_group;
coap_context_t *ctx = NULL;
coap_address_t dst_addr, src_addr;
char aei[50], cntid[50];

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const static int CONNECTED_BIT = BIT0;
unsigned int AE_BIT = BIT1;
unsigned int CNT_BIT = BIT2;
unsigned int CTRL_BIT = BIT3;

const static char *TAG = "coap_om2m_client";
static void create_ae(void *pvParameters);
static void create_container(void *pvParameters);

void adjust_current()
{
  while (1)
  {
    max30100_adjust_current();
    vTaskDelay(CURRENT_ADJUSTMENT_PERIOD_MS / portTICK_RATE_MS);
  }
}

void max30100_updater()
{
  int i;
  while (1)
  {
    max30100_update(ir_buffer, red_buffer, &data_len);
    for (i = 0; i < data_len; i++)
      printf("IR[%d]: %d\tRED[%d]: %d\n", i, ir_buffer[i], i, red_buffer[i]);
    vTaskDelay(100 / portTICK_RATE_MS);
  }
}

static void message_handler(struct coap_context_t *ctx,
                            const coap_endpoint_t *local_interface,
                            const coap_address_t *remote, coap_pdu_t *sent,
                            coap_pdu_t *received, const coap_tid_t id)
{
  char *data = NULL;
  size_t data_len;
  int has_data = coap_get_data(received, &data_len, &data);

#if defined(DEBUG_COAP)
  printf("Response class received: %d\n", COAP_RESPONSE_CLASS(received->hdr->code));
  printf("Response Code: %d\n", received->hdr->code);
  if (has_data)
    printf("Received: %s\n", data);
#endif
  if (!(xEventGroupGetBits(coap_group) & AE_BIT))
  {
    if (COAP_RESPONSE_CLASS(received->hdr->code) != COAP_RESPONSE_CLASS(COAP_RESPONSE_404))
    {                                         // 20* AE created
      xEventGroupSetBits(coap_group, AE_BIT); // AE created
      return;
    }
  }
  else if (!(xEventGroupGetBits(coap_group) & CNT_BIT))
  {
    if (COAP_RESPONSE_CLASS(received->hdr->code) != COAP_RESPONSE_CLASS(COAP_RESPONSE_404))
    {                                          // 20* Container created
      xEventGroupSetBits(coap_group, CNT_BIT); // AE created
      return;
    }
  }
  else if (!(xEventGroupGetBits(coap_group) & CTRL_BIT))
  {
    if (COAP_RESPONSE_CLASS(received->hdr->code) != COAP_RESPONSE_CLASS(COAP_RESPONSE_404))
    {                                           // 20* Controller created
      xEventGroupSetBits(coap_group, CTRL_BIT); // AE created
      return;
    }
  }
  else
  {
    if (COAP_RESPONSE_CLASS(received->hdr->code) == COAP_RESPONSE_CLASS(COAP_RESPONSE_200))
    { // Published
    }
    if (has_data)
      printf(data);
  }
}
#if 0
static void coap_retransmition_handler(void *pvParameters) {
  coap_context_t *ctx = (coap_context_t *)pvParameters;
  coap_tick_t now;
  coap_queue_t *nextpdu;

  while (1) {
    nextpdu = coap_peek_next(ctx);

    if (nextpdu) {
      if (htons(nextpdu->pdu->hdr->id) == 0) {
        coap_pop_next(ctx);
        continue;
      }
    }

    coap_ticks(&now);
    while (nextpdu && nextpdu->t <= now - ctx->sendqueue_basetime) {
      printf("retransmit id = %d\n", htons(nextpdu->pdu->hdr->id));

      coap_retransmit(ctx, coap_pop_next(ctx));
      nextpdu = coap_peek_next(ctx);
    }

    vTaskDelay(100 / portTICK_RATE_MS);
  }
}
#endif
static void coap_context_handler(void *pvParameters)
{
  coap_context_t *ctx = (coap_context_t *)pvParameters;
  fd_set readfds;
  struct timeval tv;
  int result;
  coap_queue_t *nextpdu;
  coap_tick_t now;

  set_timeout(&max_wait, wait_seconds);
  debug("timeout is set to %d seconds\n", wait_seconds);

  while (1)
  {
    FD_ZERO(&readfds);
    FD_SET(ctx->sockfd, &readfds);

    nextpdu = coap_peek_next(ctx);

    coap_ticks(&now);
    if (nextpdu &&
        nextpdu->t < min(obs_wait ? obs_wait : max_wait, max_wait) - now)
    {
      /* set timeout if there is a pdu to send */
      tv.tv_usec = ((nextpdu->t) % COAP_TICKS_PER_SECOND) * 1000000 /
                   COAP_TICKS_PER_SECOND;
      tv.tv_sec = (nextpdu->t) / COAP_TICKS_PER_SECOND;
    }
    else
    {
      /* check if obs_wait fires before max_wait */
      if (obs_wait && obs_wait < max_wait)
      {
        tv.tv_usec = ((obs_wait - now) % COAP_TICKS_PER_SECOND) * 1000000 /
                     COAP_TICKS_PER_SECOND;
        tv.tv_sec = (obs_wait - now) / COAP_TICKS_PER_SECOND;
      }
      else
      {
        tv.tv_usec = ((max_wait - now) % COAP_TICKS_PER_SECOND) * 1000000 /
                     COAP_TICKS_PER_SECOND;
        tv.tv_sec = (max_wait - now) / COAP_TICKS_PER_SECOND;
      }
    }

    result = select(ctx->sockfd + 1, &readfds, 0, 0, &tv);

    if (result < 0)
    { /* error */
      perror("select");
    }
    else if (result > 0)
    { /* read from socket */
      if (FD_ISSET(ctx->sockfd, &readfds))
      {
        coap_read(ctx); /* read received data */
        /* coap_dispatch( ctx );  /\* and dispatch PDUs from receivequeue */
      }
    }
    else
    { /* timeout */
      coap_ticks(&now);
      if (max_wait <= now)
      {
        info("timeout\n");
        continue;
      }
      if (obs_wait && obs_wait <= now)
      {
        debug("clear observation relationship\n");
        // clear_obs(ctx, ctx->endpoint, &dst); /* FIXME: handle error case
        // COAP_TID_INVALID */

        /* make sure that the obs timer does not fire again */
        obs_wait = 0;
        obs_seconds = 0;
      }
    }
  }

  coap_free_context(ctx);
  vTaskDelete(NULL);
}
static void om2m_coap_client_task(void *pvParameters)
{
  uri uri_ = {
      .entity = AE_NAME,
      .container = CONTAINER_NAME
  };

  create_ae((void *)&uri_);
  xEventGroupWaitBits(coap_group, AE_BIT, false, true, portMAX_DELAY);
  printf("AE %s created\n", AE_NAME);

  create_container((void *)&uri_);
  xEventGroupWaitBits(coap_group, CNT_BIT, false, true, portMAX_DELAY);
  printf("Container %s created\n", CONTAINER_NAME);

  uri_.container = ACTUATION;
  create_container((void *)&uri_);
  xEventGroupWaitBits(coap_group, CTRL_BIT, false, true, portMAX_DELAY);
  printf("Controller %s created\n", ACTUATION);

  char name[50];
  /* TODO:

  esp/dados node:publish pc:sub
  esp/control pc:pub node:sub

  create ae pc_pub
  create container pc_pub/hr

  create ae esp_sub

  create subscription esp_sub to pc_pub/hr
  */
#if 0
  ESP_LOGI(TAG, "Subscribing");
#if defined(TESTING) && defined(TESTE_SUB_PUB)
  printf("Testing\n");
  om2m_coap_create_subscription(ctx, dst_addr, aei, AE_NAME, CONTAINER_NAME, SUB);
#else
  om2m_coap_create_subscription(ctx, dst_addr, aei, AE_NAME, MONITOR, SUB);
#endif
#endif
#if 0
  while (1)
    vTaskDelay(1000 / portTICK_RATE_MS);
#endif
#if defined(TESTING) && defined(TESTE_PUBLISH)
  char data[] = "COMUNICATION TESTE";
#else
  char data[6];
#endif
  int i = 0;
  unsigned short int msg_id = 0;
  while (1)
  {
#if !(defined(TESTING) && defined(TESTE_PUBLISH))
    if (data_len)
      sprintf(data, "%d", ir_buffer[0]);
#endif
    sprintf(name, "%d", i);
    printf("Name: %s\n", name);
    printf("Data to send: %s\n", data);

    om2m_coap_create_content_instance(ctx, dst_addr, AE_NAME, CONTAINER_NAME,
                                      name, data, &msg_id, COAP_REQUEST_POST);
    i++;
    vTaskDelay(1000 / portTICK_RATE_MS);
  }
  while (1)
    vTaskDelay(1000 / portTICK_RATE_MS);

  vTaskDelete(NULL);
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
  switch (event->event_id)
  {
  case SYSTEM_EVENT_STA_START:
    esp_wifi_connect();
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    xEventGroupSetBits(coap_group, CONNECTED_BIT);
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    /* This is a workaround as ESP32 WiFi libs don't currently
             auto-reassociate. */
    esp_wifi_connect();
    xEventGroupClearBits(coap_group, CONNECTED_BIT);
    break;
  default:
    break;
  }
  return ESP_OK;
}

static void wifi_conn_init(void)
{
  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = WIFI_SSID,
              .password = WIFI_PASS,
              .listen_interval = 100,
          },
  };
  // WIFI_PS_MAX_MODEM
  // WIFI_PS_NONE
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
}

static void create_ae(void *pvParameters)
{
  uri *uri_ = (uri *)pvParameters;

  while (!(xEventGroupGetBits(coap_group) & AE_BIT))
  {
    ESP_LOGI(TAG, "Creating AE %s", uri_->entity);
    om2m_coap_create_ae(ctx, dst_addr, uri_->entity, 8989);
    vTaskDelay(3000 / portTICK_RATE_MS);
  }
}

static void create_container(void *pvParameters)
{
  uri *uri_ = (uri *)pvParameters;
  while (!(xEventGroupGetBits(coap_group) & CNT_BIT))
  {
    ESP_LOGI(TAG, "Creating publish container %s", uri_->container);
    om2m_coap_create_container(ctx, dst_addr, uri_->entity, uri_->container);
    vTaskDelay(3000 / portTICK_RATE_MS);
  }
}

static void init_coap(void)
{
#if defined(TESTING) && defined(DEBUG_COAP)
  printf("COAP_RESPONSE_200: %d\n", COAP_RESPONSE_CODE(200));
  printf("COAP_RESPONSE_201: %d\n", COAP_RESPONSE_CODE(201));
  printf("COAP_RESPONSE_304: %d\n", COAP_RESPONSE_CODE(304));
  printf("COAP_RESPONSE_400: %d\n", COAP_RESPONSE_CODE(400));
  printf("COAP_RESPONSE_401: %d\n", COAP_RESPONSE_CODE(401));
  printf("COAP_RESPONSE_402: %d\n", COAP_RESPONSE_CODE(402));
  printf("COAP_RESPONSE_403: %d\n", COAP_RESPONSE_CODE(403));
  printf("COAP_RESPONSE_404: %d\n", COAP_RESPONSE_CODE(404));
  printf("COAP_RESPONSE_405: %d\n", COAP_RESPONSE_CODE(405));
  printf("COAP_RESPONSE_415: %d\n", COAP_RESPONSE_CODE(415));
  printf("COAP_RESPONSE_500: %d\n", COAP_RESPONSE_CODE(500));
  printf("COAP_RESPONSE_501: %d\n", COAP_RESPONSE_CODE(501));
  printf("COAP_RESPONSE_503: %d\n", COAP_RESPONSE_CODE(503));
  printf("COAP_RESPONSE_504: %d\n", COAP_RESPONSE_CODE(504));
#endif
  // wait for AP connection
  xEventGroupWaitBits(coap_group, CONNECTED_BIT, false, true,
                      portMAX_DELAY);
  ESP_LOGI(TAG, "Connected to AP");

  coap_address_init(&src_addr);
  coap_address_init(&dst_addr);

  src_addr.addr.sin.sin_family = AF_INET;
  src_addr.addr.sin.sin_port = htons(CSE_PORT);
  src_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;

  dst_addr.addr.sin.sin_family = AF_INET;
  dst_addr.addr.sin.sin_port = htons(CSE_PORT);
  dst_addr.addr.sin.sin_addr.s_addr = inet_addr(CSE_IP);

  // create new context
  while (!ctx)
  {
    ESP_LOGI(TAG, "Creating new context");
    ctx = coap_new_context(&src_addr);

    vTaskDelay(2000 / portTICK_RATE_MS);
  }
  coap_register_response_handler(ctx, message_handler);

  // create new task for context handling
  xTaskCreate(coap_context_handler, "coap_context_handler", 8192, ctx, 5, NULL);
  ESP_LOGI(TAG, "CoAP context created");
}
void app_main(void)
{
  printf("Starting ESP\n");
  ESP_ERROR_CHECK(nvs_flash_init());
  max30100_init();
  uint8_t part;
  max30100_get_id(&part);
  printf("Part id: %d\n", part);

  coap_group = xEventGroupCreate();
  wifi_conn_init();
  init_coap();

  xTaskCreate(max30100_updater, "updater", 10000, NULL, 5, NULL);
  xTaskCreate(adjust_current, "ajdust current", 10000, NULL, 3, NULL);
  xTaskCreate(om2m_coap_client_task, "coap", 16384, NULL, 5, NULL);
}
