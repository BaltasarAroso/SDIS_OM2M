#include "om2m/coap.h"
#include "cJSON.h"

#include <string.h>

#include "tcpip_adapter.h"


int om2m_coap_create_ae(coap_context_t* ctx, coap_address_t dst_addr, char *ae_name, int ae_id) {
  int rc = -1;
  char *out = NULL;
  char poa_url[50], uri[50], originator[50];
  unsigned char content_format[2], accept[2], type[2];
  /*FIX THIS*/
  unsigned char token[] = {"fd22"};
  /*FIX THIS*/
  cJSON *payload = NULL, *ae = NULL, *poa_array = NULL, *lbls = NULL;
  tcpip_adapter_ip_info_t local_ip;
  coap_pdu_t *request = NULL;

  tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &local_ip);
  //sprintf(poa_url, "coap://"IPSTR":%d", IP2STR(&local_ip.ip), CSE_PORT);
  sprintf(poa_url, "coap://"IPSTR":%d",IP2STR(&local_ip.ip),CSE_PORT);
  sprintf(uri, "~/in-cse/%s", CSE_NAME);
  //sprintf(uri, "~/in-cse");
  sprintf(originator, "%s", CSE_ORIGINATOR);

  //create JSON payload
  payload = cJSON_CreateObject();
  poa_array = cJSON_CreateArray();
  lbls = cJSON_CreateArray();

  cJSON_AddItemToArray(poa_array, cJSON_CreateString(poa_url));
  cJSON_AddItemToArray(lbls,cJSON_CreateString("AE"));

  cJSON_AddItemToObject(payload, "m2m:ae", ae = cJSON_CreateObject());
  cJSON_AddTrueToObject(ae, "rr");
  cJSON_AddNumberToObject(ae, "api", ae_id);
  cJSON_AddStringToObject(ae, "rn", ae_name);
  cJSON_AddItemToObject(ae, "poa", poa_array);
  //cJSON_AddItemToObject(ae,"lbl",lbls);

  out = cJSON_PrintUnformatted(payload);
  cJSON_Delete(lbls);
  cJSON_Delete(poa_array);
  cJSON_Delete(payload);

  //create and send CoAP request
  request = coap_new_pdu();
  request->hdr->type = COAP_MESSAGE_NON;
  request->hdr->id   = coap_new_message_id(ctx);
  request->hdr->code = COAP_REQUEST_POST;
  coap_add_token(request, sizeof(token), token);
  coap_add_option(request, COAP_OPTION_URI_PATH, strlen(uri), (unsigned char*)uri);
  coap_add_option(request, COAP_OPTION_CONTENT_FORMAT, coap_encode_var_bytes(content_format, COAP_MEDIATYPE_APPLICATION_JSON), content_format);
  coap_add_option(request, COAP_OPTION_ACCEPT, coap_encode_var_bytes(accept, COAP_MEDIATYPE_APPLICATION_JSON), accept);
  coap_add_option(request, ONEM2M_OPTION_FR, strlen(originator), (unsigned char*)originator);
  coap_add_option(request, ONEM2M_OPTION_TY, coap_encode_var_bytes(type, 2), (unsigned char*)type);
  coap_add_data(request, strlen(out), (unsigned char*)out);
  if(request->hdr->type == COAP_MESSAGE_CON)
    rc = coap_send_confirmed(ctx, ctx->endpoint, &dst_addr, request);
  else
    rc = coap_send(ctx, ctx->endpoint, &dst_addr, request);

  coap_delete_pdu(request);
  free(out);
  return rc;
}

int om2m_coap_create_container(coap_context_t *ctx, coap_address_t dst_addr, char *ae_name, char *container_name) {
  int rc = -1;
  char *out = NULL;
  char uri[50], originator[50];
  unsigned char content_format[2], accept[2], type[2];
  unsigned char token[] = {"fd42"};
  cJSON *payload = NULL, *cnt = NULL;
  coap_pdu_t *request = NULL;

  sprintf(uri, "~/in-cse/%s/%s", CSE_NAME, ae_name);
  //sprintf(uri, "~/in-cse/%s", ae_name);
  sprintf(originator, "%s", CSE_ORIGINATOR);

  //create JSON payload
  payload = cJSON_CreateObject();
  cJSON_AddItemToObject(payload, "m2m:cnt", cnt = cJSON_CreateObject());
  cJSON_AddStringToObject(cnt, "rn", container_name);
  out = cJSON_PrintUnformatted(payload);
  cJSON_Delete(payload);

  //create and send CoAP request
  request = coap_new_pdu();
  request->hdr->type = COAP_MESSAGE_NON;
  request->hdr->id   = coap_new_message_id(ctx);
  request->hdr->code = COAP_REQUEST_POST;
  coap_add_token(request, sizeof(token), token);
  coap_add_option(request, COAP_OPTION_URI_PATH, strlen(uri), (unsigned char*)uri);
  coap_add_option(request, COAP_OPTION_CONTENT_FORMAT, coap_encode_var_bytes(content_format, COAP_MEDIATYPE_APPLICATION_JSON), content_format);
  coap_add_option(request, COAP_OPTION_ACCEPT, coap_encode_var_bytes(accept, COAP_MEDIATYPE_APPLICATION_JSON), accept);
  coap_add_option(request, ONEM2M_OPTION_FR, strlen(originator), (unsigned char*)originator);
  coap_add_option(request, ONEM2M_OPTION_TY, coap_encode_var_bytes(type, 3), (unsigned char*)type);
  coap_add_data(request, strlen(out), (unsigned char*)out);
  if(request->hdr->type == COAP_MESSAGE_CON)
    rc = coap_send_confirmed(ctx, ctx->endpoint, &dst_addr, request);
  else
    rc = coap_send(ctx, ctx->endpoint, &dst_addr, request);

  coap_delete_pdu(request);
  free(out);
  return rc;
}

int om2m_coap_create_content_instance(coap_context_t *ctx, coap_address_t dst_addr, char *ae_name, char *container_name, char *content_instance_name, char *data, unsigned short *msg_id, unsigned short msg_type) {
  int rc = -1;
  char *out = NULL;
  char uri[50], originator[50], cnf[50];
  unsigned char content_format[2], accept[2], type[2], token[2];

  cJSON *payload = NULL, *cin = NULL;
  coap_pdu_t *request = NULL;

  sprintf(uri, "~/in-cse/%s/%s/%s", CSE_NAME, ae_name, container_name);
  //sprintf(uri, "~/in-cse/%s/%s", ae_name, container_name);
  //sprintf(uri, "~/in-cse/%s", ae_name);
  sprintf(originator, "%s", CSE_ORIGINATOR);
  sprintf(cnf, "text/plain:%d", strlen(data));
  //printf("URI: %s\n",uri);
  
  //create JSON payload
  payload = cJSON_CreateObject();
  cJSON_AddItemToObject(payload, "m2m:cin", cin = cJSON_CreateObject());
  cJSON_AddStringToObject(cin, "con", data);
  cJSON_AddStringToObject(cin, "cnf", cnf);
  cJSON_AddStringToObject(cin, "rn", content_instance_name);
  out = cJSON_PrintUnformatted(payload);
  cJSON_Delete(payload);

  //create and send CoAP request
  coap_encode_var_bytes(token, htons(*msg_id));
  request = coap_new_pdu();
  request->hdr->type = msg_type;
  request->hdr->id   = htons(*msg_id);
  request->hdr->code = COAP_REQUEST_POST;
  coap_add_token(request, sizeof(token), token);
  coap_add_option(request, COAP_OPTION_URI_PATH, strlen(uri), (unsigned char*)uri);
  coap_add_option(request, COAP_OPTION_CONTENT_FORMAT, coap_encode_var_bytes(content_format, COAP_MEDIATYPE_APPLICATION_JSON), content_format);
  coap_add_option(request, COAP_OPTION_ACCEPT, coap_encode_var_bytes(accept, COAP_MEDIATYPE_APPLICATION_JSON), accept);
  coap_add_option(request, ONEM2M_OPTION_FR, strlen(originator), (unsigned char*)originator);
  coap_add_option(request, ONEM2M_OPTION_TY, coap_encode_var_bytes(type, 4), (unsigned char*)type);
  coap_add_data(request, strlen(out), (unsigned char*)out);

  if(request->hdr->type == COAP_MESSAGE_CON)
    rc = coap_send_confirmed(ctx, ctx->endpoint, &dst_addr, request);
  else {
    rc = coap_send(ctx, ctx->endpoint, &dst_addr, request);
    coap_delete_pdu(request);
  }

  *msg_id = *msg_id + 1;
  free(out);
  return rc;
}

int om2m_coap_create_subscription(coap_context_t *ctx, coap_address_t dst_addr, char *ae_name, char* container_name, char *ae_monitor_name, char *sub_name) {
  int rc = -1;
  char *out = NULL;
  char uri[50], originator[50], nu_uri[50];
  unsigned char content_format[2], accept[2], type[2], token[2];
  unsigned short msg_id = 0;
  cJSON *payload = NULL, *sub = NULL, *nu_array = NULL;
  coap_pdu_t *request = NULL;

  sprintf(nu_uri, "/in-cse/%s/%s", CSE_NAME, ae_monitor_name);
  sprintf(uri, "~/in-cse/%s/%s/%s", CSE_NAME, ae_name, container_name);
  /*sprintf(nu_uri, "/in-cse/%s/%s",ae_name, ae_monitor_name);
  sprintf(uri, "~/in-cse/%s",rid);*/
  sprintf(originator, "%s", CSE_ORIGINATOR);

  //create JSON payload
  payload = cJSON_CreateObject();
  nu_array = cJSON_CreateArray();
  cJSON_AddItemToArray(nu_array, cJSON_CreateString(nu_uri));
  cJSON_AddItemToObject(payload, "m2m:sub", sub = cJSON_CreateObject());
  cJSON_AddStringToObject(sub, "rn", sub_name);
  cJSON_AddNumberToObject(sub, "nct", 2);
  cJSON_AddItemToObject(sub, "nu", nu_array);
  out = cJSON_PrintUnformatted(payload);
  cJSON_Delete(payload);

  //create and send CoAP request
  msg_id = coap_new_message_id(ctx);
  coap_encode_var_bytes(token, msg_id);

  request = coap_new_pdu();
  request->hdr->type = COAP_MESSAGE_NON;
  request->hdr->id   = msg_id;
  request->hdr->code = COAP_REQUEST_POST;
  coap_add_token(request, sizeof(token)-1, token);
  coap_add_option(request, COAP_OPTION_URI_PATH, strlen(uri), (unsigned char*)uri);
  coap_add_option(request, COAP_OPTION_CONTENT_FORMAT, coap_encode_var_bytes(content_format, COAP_MEDIATYPE_APPLICATION_JSON), content_format);
  coap_add_option(request, COAP_OPTION_ACCEPT, coap_encode_var_bytes(accept, COAP_MEDIATYPE_APPLICATION_JSON), accept);
  coap_add_option(request, ONEM2M_OPTION_FR, strlen(originator), (unsigned char*)originator);
  coap_add_option(request, ONEM2M_OPTION_TY, coap_encode_var_bytes(type, 23), (unsigned char*)type);
  coap_add_data(request, strlen(out), (unsigned char*)out);
  if(request->hdr->type == COAP_MESSAGE_CON)
    rc = coap_send_confirmed(ctx, ctx->endpoint, &dst_addr, request);
  else
    rc = coap_send(ctx, ctx->endpoint, &dst_addr, request);

  coap_delete_pdu(request);
  free(out);
  return rc;
}
