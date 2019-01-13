#include "MQTTClient.h"

#define CSE_NAME  		"dartes"

#define MQTT_FROM		"admin:admin"
#define MQTT_BROKER_IP 		"192.168.106.131"
#define MQTT_BROKER_PORT	1883
#define MQTT_QOS		QOS0
#define MQTT_SEND_BUF_SIZE	2000
#define MQTT_READ_BUF_SIZE	2000
#define MQTT_TIMEOUT		30000

static const char *TAG = "om2m_mqtt";

int om2m_mqtt_create_ae(MQTTClient* client, char *ae_name, int ae_id);
int om2m_mqtt_create_container(MQTTClient* client, char *ae_name, char *container_name);
int om2m_mqtt_create_content_instance(MQTTClient* client, char *ae_name, char *container_name, char *content_instance_name, char *data);
int om2m_create_subscription(MQTTClient* client, char *ae_name, char* container_name, char *ae_monitor_name, char *sub_name);


/*char* get_create_application_payload(char* fr, char* to, int rqi, char* poa, int api, char* rn);
char* get_create_container_payload(char* fr, char *to, int rqi, char* rn);
void mqtt_onem2m_publish(MQTTClient* client, char* topic, char* payload);

char* get_hr_json(int size);
char* get_create_content_hr_payload(char* fr, char *to, int rqi, char* cnf, char* con, char* rn);
char* get_create_subscription_payload(char* fr, char *to, int rqi, char* nu, char* rn, int nct);*/

