#ifndef _PROJECT_CONFIG_H_
#define _PROJECT_CONFIG_H_

// GPIO0 pins
#define	PIN_GPIO_ONBOARD_BUTTON	0
#define	PIN_GPIO_RELAY_1		16	
#define	PIN_GPIO_RELAY_2		17	
#define	PIN_GPIO_ONBOARD_LED	23	

// DEBUGGING OPTIONS
#define	_VERBOSE_				0
#define	_SELF_SUBSCRIBE_		1	// if _SELF_SUBSCRIBE_ then the device sends the subscribe request to retrieve their own published messages (meant for testing pourposes)

// WiFi
#define	WIFI_SSID				"YOUR_SSID"
#define	WIFI_PASSWORD			"THE_PASSWORD_OF_YOUR_SSID"

// 	MQTT Server (Mosquitto)		
#define MQTT_HOST_IP_ADDR 		"192.168.1.2"
#define MQTT_HOST_IP_PORT 		1883

// Endianness (little-endian, big-endian) 
#define ENDIANNESS LITTLE_ENDIAN

#if ENDIANNESS == LITTLE_ENDIAN
#define ENDIAN(a) ( (((uint16_t)a<<8) & 0xFF00) | (((uint16_t)a>>8) & 0x00FF) )
#else
#define ENDIAN(a) (a)	
#endif

// Rest API SECURE KEY
#define SERVER_SECURE_KEY    	"qWpJnwA0crlmgv"

// MQTT PINGREQ period (seconds)
#define	MQTT_PINGREQ_TIME		60

// THIS DEVICE MQTT ID  
#define DEVICE_MQTT_NAME		"relayX2board"

#endif
// END OF FILE