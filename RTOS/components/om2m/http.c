#include "om2m/http.h"
#include "cJSON.h"

#include <string.h>

#include "tcpip_adapter.h"


static int om2m_http_post(int clientfd, char* url, char *payload, int ty) {
  char *buf = malloc(strlen(payload)+sizeof(char)*250);
  memset(buf, 0, strlen(payload)+250);

  if(buf == NULL)
    return -1;

  else {
    sprintf(buf, "POST %s HTTP/1.1\r\nX-M2M-Origin: admin:admin\r\nContent-Type: application/json;ty=%d\r\nAccept: application/json\r\nContent-Length: %d\r\nHost: %s:%d\r\nConnection: Keep-Alive\r\nUser-Agent: ESP8266\r\n\r\n%s", url, ty, strlen(payload), CSE_IP, CSE_PORT_HTTP, payload);
    printf("sending...!\n");
    int res = write(clientfd, buf, strlen(buf));

    free(buf);
    return res;
  }
}

int om2m_http_ok(int clientfd) {
  char buf[] = {"HTTP/1.1 200 OK\r\nTransfer-encoding: chunked\r\n\r\n0\r\n\r\n"};

  printf("%s\n", buf);
  return send(clientfd, buf, strlen(buf), 0);
}

int om2m_http_delete(int clientfd, char *url) {
  char *buf = malloc(sizeof(char)*300);
  memset(buf, 0, sizeof(char)*300);

  if(buf == NULL)
    return -1;

  else {
    sprintf(buf, "DELETE %s HTTP/1.1\r\nX-M2M-Origin: admin:admin\r\nAccept: application/json\r\nHost: %s:%d\r\nConnection: Keep-Alive\r\nUser-Agent: ESP8266\r\n\r\n", url, CSE_IP, CSE_PORT_HTTP);
    int res = send(clientfd, buf, strlen(buf), 0);
    free(buf);
    return res;
  }
}

int om2m_http_create_ae(int clientfd, char *ae_name, int ae_id) {
  cJSON *payload = NULL;
  cJSON *ae = NULL;
  cJSON *poa_array = NULL;

  char *out = NULL;

  tcpip_adapter_ip_info_t local_ip;
  tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &local_ip);

  char poa_url[50];
  //sprintf(poa_url, "http://"IPSTR":%d", IP2STR(&local_ip.ip), HTTP_SERVER_PORT);
  sprintf(poa_url, "http://192.168.106.167:96");

  payload = cJSON_CreateObject();
  poa_array = cJSON_CreateArray();
  cJSON_AddItemToArray(poa_array, cJSON_CreateString(poa_url));

  cJSON_AddItemToObject(payload, "m2m:ae", ae = cJSON_CreateObject());
  cJSON_AddTrueToObject(ae, "rr");
  cJSON_AddNumberToObject(ae, "api", ae_id);
  cJSON_AddStringToObject(ae, "rn", ae_name);
  cJSON_AddItemToObject(ae, "poa", poa_array);
  out = cJSON_PrintUnformatted(payload);
  cJSON_Delete(payload);

  char url[50];
  sprintf(url, "/~/in-cse/%s", CSE_NAME);

  int res = om2m_http_post(clientfd, url, out, 2);
  free(out);

  return res;
}


int om2m_http_create_container(int clientfd, char *ae_name, char *container_name) {
  cJSON *payload = NULL;
  cJSON *cnt = NULL;

  char *out = NULL;

  payload = cJSON_CreateObject();
  cJSON_AddItemToObject(payload, "m2m:cnt", cnt = cJSON_CreateObject());
  cJSON_AddStringToObject(cnt, "rn", container_name);
  out = cJSON_PrintUnformatted(payload);
  cJSON_Delete(payload);

  char url[50];
  sprintf(url, "/~/in-cse/%s/%s", CSE_NAME, ae_name);

  int res = om2m_http_post(clientfd, url, out, 3);
  free(out);

  return res;
}

int om2m_http_create_content_instance(int clientfd, char *ae_name, char *container_name, char *content_instance_name, char *data) {
  cJSON *payload = NULL;
  cJSON *cin = NULL;

  char *out = NULL;
  char cnf[50], url[50];
  sprintf(url, "/~/in-cse/%s/%s/%s", CSE_NAME, ae_name, container_name);
  sprintf(cnf, "text/plain:%d", strlen(data));

  payload = cJSON_CreateObject();
  cJSON_AddItemToObject(payload, "m2m:cin", cin = cJSON_CreateObject());
  cJSON_AddStringToObject(cin, "con", data);
  cJSON_AddStringToObject(cin, "cnf", cnf);
  cJSON_AddStringToObject(cin, "rn", content_instance_name);
  out = cJSON_PrintUnformatted(payload);
  cJSON_Delete(payload);


  int res = om2m_http_post(clientfd, url, out, 4);
  free(out);

  return res;
  //{"m2m:cin":{"pc":"cenas_teste","con":"8001","cnf":"application/json","rn":"8001"}}
}

int om2m_http_create_subscription(int clientfd, char *ae_name, char* container_name, char *ae_monitor_name, char *sub_name) {
  cJSON *payload = NULL;
  cJSON *sub = NULL;
  cJSON *nu_array = NULL;

  char *out = NULL;

  char nu_url[50];
  sprintf(nu_url, "/in-cse/%s/%s", CSE_NAME, ae_monitor_name);

  payload = cJSON_CreateObject();
  nu_array = cJSON_CreateArray();
  cJSON_AddItemToArray(nu_array, cJSON_CreateString(nu_url));

  cJSON_AddItemToObject(payload, "m2m:sub", sub = cJSON_CreateObject());
  cJSON_AddStringToObject(sub, "rn", sub_name);
  cJSON_AddNumberToObject(sub, "nct", 2);
  cJSON_AddItemToObject(sub, "nu", nu_array);
  out = cJSON_PrintUnformatted(payload);
  cJSON_Delete(payload);

  char url[50];
  sprintf(url, "/~/in-cse/%s/%s/%s", CSE_NAME, ae_name, container_name);

  int res = om2m_http_post(clientfd, url, out, 23);
  free(out);

  return res;
}
