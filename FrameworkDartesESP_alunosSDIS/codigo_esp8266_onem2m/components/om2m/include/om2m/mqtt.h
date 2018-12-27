#include "MQTTClient.h"
#include "malloc.h"

#define CSE_NAME  		"dartes"

#define MQTT_FROM		"admin:admin"
#define MQTT_BROKER_IP 		"192.168.106.xxx"//"192.168.43.175"//192.168.106.131"
#define MQTT_BROKER_PORT	1883
#define MQTT_QOS		QOS0
#define MQTT_SEND_BUF_SIZE	2000
#define MQTT_READ_BUF_SIZE	2000
#define MQTT_TIMEOUT		900000


int om2m_mqtt_create_ae(MQTTClient* client, char *ae_name, int ae_id);
int om2m_mqtt_create_container(MQTTClient* client, char *ae_name, char *container_name);
int om2m_mqtt_create_content_instance(MQTTClient* client, char *ae_name, char *container_name, char *content_instance_name, char *data);
int om2m_create_subscription(MQTTClient* client, char *ae_name, char* container_name, char *ae_monitor_name, char *sub_name);
