#include "om2m/mqtt.h"
#include "cJSON.h"

#include <string.h>

#include "tcpip_adapter.h"


int mqtt_publish(MQTTClient* client, char* payload) {
  MQTTMessage message;

  message.qos = MQTT_QOS;
  message.retained = 0;
  message.payload = payload;
  message.payloadlen = strlen(message.payload);

  tcpip_adapter_ip_info_t local_ip;
  tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &local_ip);
  char topic[70];
  sprintf(topic, "/oneM2M/req/ESP8266_"IPSTR"/in-cse/json", IP2STR(&local_ip.ip));

  int rc = -1;
  rc = MQTTPublish(client, topic, &message);

  return rc;
}

int om2m_mqtt_create_ae(MQTTClient* client, char *ae_name, int ae_id) {
  cJSON *payload = NULL;
  cJSON *poa_array = NULL;

  cJSON *rqp = NULL;
  cJSON *pc = NULL;
  cJSON *ae = NULL;

  char *out = NULL;

  char *to = malloc(sizeof(CSE_NAME)+10);
  sprintf(to, "/in-cse/%s", CSE_NAME);

  tcpip_adapter_ip_info_t local_ip;
  tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &local_ip);
  char poa[70];
  sprintf(poa, "mqtt://%s:%d/oneM2M/req/ESP8266_"IPSTR"/json", MQTT_BROKER_IP, MQTT_BROKER_PORT, IP2STR(&local_ip.ip));
  poa_array = cJSON_CreateArray();
  cJSON_AddItemToArray(poa_array, cJSON_CreateString(poa));


  payload = cJSON_CreateObject();
  cJSON_AddItemToObject(payload, "m2m:rqp", rqp = cJSON_CreateObject());
  cJSON_AddStringToObject(rqp, "m2m:fr", MQTT_FROM);
  cJSON_AddStringToObject(rqp, "m2m:to", to);
  cJSON_AddNumberToObject(rqp, "m2m:op", 1);
  //cJSON_AddNumberToObject(rqp, "m2m:rqi", rqi);
  cJSON_AddItemToObject(rqp, "m2m:pc", pc = cJSON_CreateObject());
  cJSON_AddItemToObject(pc, "m2m:ae", ae = cJSON_CreateObject());
  cJSON_AddStringToObject(ae, "rr", "true");
  cJSON_AddItemToObject(ae, "poa", poa_array);
  cJSON_AddNumberToObject(ae, "api", ae_id);
  cJSON_AddStringToObject(ae, "rn", ae_name);
  cJSON_AddNumberToObject(rqp, "m2m:ty", 2);

  out = cJSON_PrintUnformatted(payload);

  int res = mqtt_publish(client, out);

  cJSON_Delete(payload);
  free(out);
  free(to);

  return res;
}

int om2m_mqtt_create_container(MQTTClient* client, char *ae_name, char *container_name) {

  cJSON *payload = NULL;
  cJSON *rqp = NULL;
  cJSON *pc = NULL;
  cJSON *cnt = NULL;

  char *out = NULL;

  char *to = malloc(sizeof(CSE_NAME)+20);
  sprintf(to, "/in-cse/%s/%s", CSE_NAME, ae_name);

  payload = cJSON_CreateObject();

  cJSON_AddItemToObject(payload, "m2m:rqp", rqp = cJSON_CreateObject());
  cJSON_AddStringToObject(rqp, "m2m:fr", MQTT_FROM);
  cJSON_AddStringToObject(rqp, "m2m:to", to);
  cJSON_AddNumberToObject(rqp, "m2m:op", 1);
  //cJSON_AddNumberToObject(rqp, "m2m:rqi", rqi);
  cJSON_AddItemToObject(rqp, "m2m:pc", pc = cJSON_CreateObject());
  cJSON_AddItemToObject(pc, "m2m:cnt", cnt = cJSON_CreateObject());
  cJSON_AddStringToObject(cnt, "rn", container_name);
  cJSON_AddNumberToObject(rqp, "m2m:ty", 3);

  out = cJSON_PrintUnformatted(payload);

  int res = mqtt_publish(client, out);

  cJSON_Delete(payload);
  free(out);
  free(to);

  return res;
}

int om2m_mqtt_create_content_instance(MQTTClient* client, char *ae_name, char *container_name, char *content_instance_name, char *data) {
  cJSON *payload = NULL;
  cJSON *rqp = NULL;
  cJSON *pc = NULL;
  cJSON *cin = NULL;

  char *out = NULL;
  char *to = malloc(sizeof(CSE_NAME)+30);
  char cnf[20];

  sprintf(to, "/in-cse/%s/%s/%s", CSE_NAME, ae_name, container_name);
  sprintf(cnf, "text/plain:%d", sizeof(data));

  payload = cJSON_CreateObject();
  cJSON_AddItemToObject(payload, "m2m:rqp", rqp = cJSON_CreateObject());
  cJSON_AddStringToObject(rqp, "m2m:fr", MQTT_FROM);
  cJSON_AddStringToObject(rqp, "m2m:to", to);
  cJSON_AddNumberToObject(rqp, "m2m:op", 1);
  cJSON_AddItemToObject(rqp, "m2m:pc", pc = cJSON_CreateObject());
  cJSON_AddItemToObject(pc, "m2m:cin", cin = cJSON_CreateObject());
  cJSON_AddStringToObject(cin, "con", data);

  cJSON_AddStringToObject(cin, "cnf", cnf);
  cJSON_AddStringToObject(cin, "rn", content_instance_name);
  cJSON_AddNumberToObject(rqp, "m2m:ty", 4);

  out = cJSON_PrintUnformatted(payload);

  int res = mqtt_publish(client, out);

  cJSON_Delete(payload);
  free(out);
  free(to);

  return res;
}

int om2m_create_subscription(MQTTClient* client, char *ae_name, char* container_name, char *ae_monitor_name, char *sub_name) {
  cJSON *payload = NULL;
  cJSON *nu_array = NULL;
  cJSON *rqp = NULL;
  cJSON *pc = NULL;
  cJSON *sub = NULL;

  char *out = NULL;

  char *to = malloc(sizeof(CSE_NAME)+30);
  sprintf(to, "/in-cse/%s/%s/%s", CSE_NAME, ae_name, container_name);

  char nu_url[50];
  sprintf(nu_url, "/in-cse/%s/%s", CSE_NAME, ae_monitor_name);

  payload = cJSON_CreateObject();
  nu_array = cJSON_CreateArray();
  cJSON_AddItemToArray(nu_array, cJSON_CreateString(nu_url));

  cJSON_AddItemToObject(payload, "m2m:rqp", rqp = cJSON_CreateObject());
  cJSON_AddStringToObject(rqp, "m2m:fr", MQTT_FROM);
  cJSON_AddStringToObject(rqp, "m2m:to", to);
  cJSON_AddNumberToObject(rqp, "m2m:op", 1);
  //cJSON_AddNumberToObject(rqp, "m2m:rqi", rqi);
  cJSON_AddItemToObject(rqp, "m2m:pc", pc = cJSON_CreateObject());
  cJSON_AddItemToObject(pc, "m2m:sub", sub = cJSON_CreateObject());
  cJSON_AddItemToObject(sub, "nu", nu_array);
  cJSON_AddStringToObject(sub, "rn", sub_name);
  cJSON_AddNumberToObject(sub, "nct", 2);
  cJSON_AddNumberToObject(rqp, "m2m:ty", 23);

  out = cJSON_PrintUnformatted(payload);

  int res = mqtt_publish(client, out);

  cJSON_Delete(payload);
  free(out);
  free(to);

  return res;
}
