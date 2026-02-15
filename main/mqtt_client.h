#ifndef _MQTT_CLIENT_H_
#define _MQTT_CLIENT_H_

#define QUEUE_SIZE				8
#define PUBLISHDATA_MAX_SIZE 	256

typedef struct MQTTPublishQueue_s {
	char data[QUEUE_SIZE][PUBLISHDATA_MAX_SIZE];
	int n[QUEUE_SIZE];	// n = bytes stored in data[]
	int in;
	int out;
} MQTTPublishQueue_t;

void MQTTPublishQueue_clear(void);
int MQTTPublishQueue_n(void);
int MQTTPublishQueue_push(char *data, int n);
int MQTTPublishQueue_pop(char **data);
void MQTTPublishQueue_flush(void);

void MQTT_client_create( int (*callback) (int, void*), int (*pcallback) (char*), UBaseType_t uxPriority);
bool MQTT_is_connected(void);
int MQTT_publish(char*);

#endif
// END OF FILE