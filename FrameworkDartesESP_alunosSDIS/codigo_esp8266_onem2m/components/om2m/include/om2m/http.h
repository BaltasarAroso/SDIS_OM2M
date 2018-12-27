#include "malloc.h"

#include <netdb.h>
#include <sys/socket.h>


#define CSE_IP 			"192.168.106.xxx"//"192.168.43.175"//192.168.106.131"
#define CSE_PORT_HTTP 		8080
#define CSE_NAME 		"dartes"
#define CSE_ORIGINATOR 		"admin:admin"

int om2m_http_ok(int clientfd);
int om2m_http_create_ae(int clientfd, char *ae_name, int ae_id);
int om2m_http_create_container(int clientfd, char *ae_name, char *container_name);
int om2m_http_create_content_instance(int clientfd, char *ae_name, char *container_name, char *content_instance_name, char *data);
int om2m_http_create_subscription(int clientfd, char *ae_name, char* container_name, char *ae_monitor_name, char *sub_name);




