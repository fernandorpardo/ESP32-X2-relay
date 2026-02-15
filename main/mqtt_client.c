/** ************************************************************************************************
 *	MQTT client
 *  (c) Fernando R (iambobot.com)
 *
 * 	1.0.0 - December 2025 - created
 *  1.0.1 - January 2026
 *
 ** ************************************************************************************************
**/

#include <stdio.h>		// fprintf (mqtt_decode())
#include <string.h>		// memcpy
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "config.h"
#include "cstr.h"
#include "network_wifi.h"
#include "network_tcpclient.h"
#include "mqtt.h"
#include "mqtt_client.h"

extern char IPaddr[32];
extern char MACaddr[32];

static bool MQTT_status_connected;
static bool MQTT_status_subscribe_send;

static int (*PUBLISHCallback) (char*)= 0;

/**
---------------------------------------------------------------------------------------------------
		
								   Mosquitto TCP Client

---------------------------------------------------------------------------------------------------
**/
// Manage connection to Mosquito broker
// and MQTT response messages
// CONNACK
// PINGRESP
// SUBACK & PUBLISH (in case)
void xTask_MQTT_listener(void *pvParameters)
{
	// network_s_tcp_connected= false;
	char rx_buffer[512];

	while (1)
	{
		//WiFi is NOT connected
		if(!network_wifi_is_connected())
		{
			// Wait for wifi
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
		// WiFi is connected
		else			
		{
			if(!network_tcp_is_connected())
			{
				if( network_tcp_connect() == 0)
				{
					MQTT_status_connected= false;
					MQTT_status_subscribe_send= false;
					fprintf(stdout,"CLIENT CONNECTED\n");
				}
				else
				{
					fprintf(stdout, "[xTask_MQTT_listener] network_tcp_connect FAILED !!! Retry in 3 sec...\n");
					fflush(stdout);	
					vTaskDelay(3000 / portTICK_PERIOD_MS);
				}
			}
			else
			{
				// Process TCP messages
				int n;
				if( (n=network_tcp_receive(rx_buffer, sizeof(rx_buffer))) > 0 )
				{
					char Control_Packet_type= 0;
					mqtt_decode(&Control_Packet_type, rx_buffer, n);
					// if TCP message is an MQTT messages (Control_Packet_type != 0)
					// the process it
					if(Control_Packet_type == CONNACK)
					{
						// (3) CONNECTION MQTT
						MQTT_status_connected= true;
						MQTT_status_subscribe_send= false;		
						fprintf(stdout,"(3) CONNECTION MQTT\n");
						
						// Report (MQTT PUBLISH) IP address
						char mess[128];
						snprintf(mess, sizeof(mess), "{\"IP\":\"%s\",\"MAC\":\"%s\"}", IPaddr, MACaddr);
						mqtt_publish(network_tcp_send, DEVICE_MQTT_NAME"/info", mess);							
					}
					else if(Control_Packet_type == PINGRESP)
					{
						// Do nothing					
					}

					
					// if _SELF_SUBSCRIBE_ then the device gets their own published messages (meant for testing pourposes)
					else if(Control_Packet_type == SUBACK)
					{
						// (4) CONNECTION SUBSCRIBED	
						fprintf(stdout,"(4) CONNECTION SUBSCRIBED\n");						
					}					
					else if(Control_Packet_type == PUBLISH)
					{
						fprintf(stdout, "\n\nPUBLISH\n");
						char topic[64];
						char payload[256];
						mqtt_payload(&Control_Packet_type, rx_buffer, n, topic, payload, sizeof(payload));	
						fprintf(stdout,"topic   %s\n", topic);			
						fprintf(stdout,"payload %s\n", payload);
						// is it for me (this device)
						if(strcmp(topic, DEVICE_MQTT_NAME"/set") == 0)
						{
							fprintf(stdout,"IT IS FOR ME\n");
							fflush(stdout);
							if(_VERBOSE_)
							{
								cstr_dump(rx_buffer, n);
								printf("\n");
								fflush(stdout);	
							}
							if(PUBLISHCallback) PUBLISHCallback(payload);
						}	
					}
				}
				vTaskDelay(20 / portTICK_PERIOD_MS);
			}
		} // wait for WiFi
	} // while task
} // xTask_MQTT_listener()

// Send MQTT messages
// CONNECT
// PINGREQ
// PUBLISH from queue
void xTask_MQTT_client(void *pvParameters)
{
	int count=0;
	MQTTPublishQueue_clear();
	while(1)
	{
		// PUBLISH from queue
		while( MQTTPublishQueue_n() > 0 )
		{
			char *data;
			int n= MQTTPublishQueue_pop(&data);
			if(n && MQTT_status_connected && network_tcp_is_connected())
			{
				mqtt_publish(network_tcp_send, DEVICE_MQTT_NAME"/info", data);
				fprintf(stdout, "\n[xTask_MQTT_client] Buffer PUBLISH %s", data); 
				fflush(stdout);
			}
			MQTTPublishQueue_flush();
		}
		
		if(network_tcp_is_connected())
		{
			if(!MQTT_status_connected)
			{
				if(count<=0)
				{
					printf("\n[xTask_MQTT_client] Sending MQTT CONNECT"); 
					fflush(stdout);
					mqtt_connect(network_tcp_send);
					//vTaskDelay(1000 / portTICK_PERIOD_MS);
					count= 3;
				}
				else count --;
				
				MQTT_status_subscribe_send= false;
			}
			else if(!MQTT_status_subscribe_send)
			{
				MQTT_status_subscribe_send= true;
				if(_SELF_SUBSCRIBE_)
				{
					printf("\n[xTask_MQTT_client] Sending MQTT mqtt_subscribe"); 
					fflush(stdout);
					mqtt_subscribe(network_tcp_send, 1, DEVICE_MQTT_NAME"/set");
					fprintf(stdout,"mqtt_subscribe\n");
				}
				count= MQTT_PINGREQ_TIME;
			}
			else
			{
				if(count<=0)
				{
					printf("\n[xTask_MQTT_client] Sending MQTT PING"); 
					fflush(stdout);
					mqtt_ping(network_tcp_send);
					count= MQTT_PINGREQ_TIME;
				}
				else count --;
			}
		}
		else 
		{
			count= 0;
			MQTT_status_connected= false;
			MQTT_status_subscribe_send= false;
		}
		
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
} // mqtt_client()		


bool MQTT_is_connected(void)
{
	return MQTT_status_connected;
} // MQTT_is_connected()


void MQTT_client_create( int (*callback) (int, void*), int (*pcallback) (char*), UBaseType_t uxPriority)
{
	MQTT_status_connected= false;
	MQTT_status_subscribe_send= false;
	PUBLISHCallback= pcallback;
	network_tcp_init(callback);
	xTaskCreate(xTask_MQTT_listener, "TCP", 8*1024, NULL, uxPriority, NULL);
	xTaskCreate(xTask_MQTT_client, "MQTT", 8*1024, NULL, uxPriority + 1, NULL);
} // MQTT_client_create


int MQTT_publish(char* data)
{
	return MQTTPublishQueue_push(data, strlen(data));
} // MQTT_publish()


/**
---------------------------------------------------------------------------------------------------
		
								   MQTT Publish Queue

---------------------------------------------------------------------------------------------------
**/

MQTTPublishQueue_t MQTTPublishQueue;

void MQTTPublishQueue_clear(void)
{
	MQTTPublishQueue.out= MQTTPublishQueue.in = 0;
} // MQTTPublishQueue_clear()

// n = queued messages
int MQTTPublishQueue_n(void)
{
	int n= MQTTPublishQueue.in - MQTTPublishQueue.out;
	if(n<0) n= QUEUE_SIZE + n;
	return n;
} // MQTTPublishQueue_n

// push n bytes
int MQTTPublishQueue_push(char *data, int n)
{
	int in;
	if(MQTTPublishQueue_n() < QUEUE_SIZE - 1) in= MQTTPublishQueue.in;
	// no free data buffer
	else  
	{ 
		fprintf(stdout, "\n\n******* DATA QUEUE IS FULL\n"); 
		fflush(stdout); 
		return -1;
	}
	memcpy(MQTTPublishQueue.data[in], data, n);
	MQTTPublishQueue.n[in]= n;
	MQTTPublishQueue.in= (MQTTPublishQueue.in + 1) % QUEUE_SIZE;
	return n;
} // MQTTPublishQueue_push

int MQTTPublishQueue_pop(char **data)
{
	int out= MQTTPublishQueue.out;
	*data= MQTTPublishQueue.data[out];
	int n= MQTTPublishQueue.n[out];
	return n;
} // MQTTPublishQueue_pop

void MQTTPublishQueue_flush(void)
{
	MQTTPublishQueue.out= (MQTTPublishQueue.out + 1) % QUEUE_SIZE;
} // MQTTPublishQueue_flush


// END OF FILE