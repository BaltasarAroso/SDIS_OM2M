#include <coap/coap.h>

#define ONEM2M_OPTION_FR	256
#define ONEM2M_OPTION_RQI	257
#define ONEM2M_OPTION_NM	258
#define ONEM2M_OPTION_OT	259
#define ONEM2M_OPTION_RQET	260
#define ONEM2M_OPTION_RSET	261
#define ONEM2M_OPTION_OET	262
#define ONEM2M_OPTION_RTURI	263
#define ONEM2M_OPTION_EC	264
#define ONEM2M_OPTION_RSC	265
#define ONEM2M_OPTION_GID	266
#define ONEM2M_OPTION_TY	267

//#define COAP_MESSAGE_TYPE_RQST 	COAP_MESSAGE_NON

#define CSE_IP 			"192.168.xxx.xxx" // IP broker
#define CSE_PORT 		5683
#define CSE_NAME 		"dartes"
#define CSE_ORIGINATOR 	"admin:admin"

//extern uint8_t msg_type = COAP_MESSAGE_NON;

int om2m_coap_create_ae(coap_context_t* ctx, coap_address_t dst_addr, char *ae_name, int ae_id);
int om2m_coap_create_container(coap_context_t *ctx, coap_address_t dst_addr, char *ae_name, char *container_name);
int om2m_coap_create_content_instance(coap_context_t *ctx, coap_address_t dst_addr, char *ae_name, char *container_name, char *content_instance_name, char *data, unsigned short *msg_id, unsigned short msg_type);
int om2m_coap_create_subscription(coap_context_t *ctx, coap_address_t dst_addr, char *ae_name, char* container_name, char *ae_monitor_name, char *sub_name);



