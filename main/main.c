/** ************************************************************************************************
 *	relayX2board  
 *  (c) Fernando R (iambobot.com)
 * 	1.0.0 - February 2026
 * 
 *
 ** ************************************************************************************************
**/

/**
To generate the code 

. $HOME/esp/esp-idf/export.sh
cd ~/esp/relayX2board

idf.py set-target esp32

idf.py menuconfig
	
idf.py build


Execute
idf.py -p /dev/ttyUSB0 flash

idf.py -p /dev/ttyUSB0 monitor

idf.py -p /dev/ttyUSB0 flash monitor

Device info
esptool.py --port /dev/ttyUSB0 flash_id


Monitor

press Ctrl+a to print SW info
To exit IDF monitor use the shortcut Ctrl+]
**/
/**
TEST RESP-API
		curl  -X GET http://192.168.1.137:80 -d '{"device":"led","set":"on","key":"qWpJnwA0crlmgv"}'	
		curl  -X GET http://192.168.1.137:80 -d '{"device":"led","set":"off","key":"qWpJnwA0crlmgv"}'	
		curl  -X GET http://192.168.1.137:80 -d '{"device":"uno","set":"on","key":"qWpJnwA0crlmgv"}'	
		curl  -X GET http://192.168.1.137:80 -d '{"device":"uno","set":"off","key":"qWpJnwA0crlmgv"}'	
		curl  -X GET http://192.168.1.137:80 -d '{"device":"dos","set":"on","key":"qWpJnwA0crlmgv"}'	
		curl  -X GET http://192.168.1.137:80 -d '{"device":"dos","set":"off","key":"qWpJnwA0crlmgv"}'	


TEST MQTT
	SUBSCRIBE TO THE MESSAGES FROM THIS DEVICE
		mosquitto_sub -d -t 'relayX2board/info'
		mosquitto_sub -d -t 'relayX2board/set'

	Trun on/off RELAY 1
		mosquitto_pub -t "relayX2board/set" -m '{"device":"uno","set":"on"}'
		mosquitto_pub -t "relayX2board/set" -m '{"device":"uno","set":"off"}'
**/

#include <stdio.h>
#include <string.h>			// memset
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "soc/gpio_sig_map.h"
#include "esp_timer.h"

#include "config.h"
#include "cstr.h"
#include "gpio_lib.h"
#include "mqtt.h"
#include "mqtt_client.h"
#include "network_wifi.h"
#include "network_tcpclient.h"
#include "network_webserver.h"


#define PROJECT_NAME		"relayX2board"
#define PROJECT_LOCATION 	"esp/relayX2board"
#define PROJECT_VERSION		"01.00.00"

static const char *TAG = "relay";

/** ***********************************************************************************************
	This SW
	
	Tasks : 
		MQTT 
		TCP 
		RestAPI 
		gpio_task

	Priority - a lower numerical value indicates a lower priority, and a higher number indicates a higher priority
	UART tasks have the highest priotity
	The priority goes from 0 to ( configMAX_PRIORITIES - 1 ), where configMAX_PRIORITIES is defined within FreeRTOSConfig.h
	The idle task has priority zero (tskIDLE_PRIORITY).


*********************************************************************************************** **/

bool led_is_on;
bool relay_UNO_is_on;
bool relay_DOS_is_on;


/**
---------------------------------------------------------------------------------------------------
		
								   UPTIME

---------------------------------------------------------------------------------------------------
**/
uint32_t timeOnBoot;	// seconds

char* upTime(char *uptime_str)
{
    uint32_t timeNow = (uint32_t) (esp_timer_get_time() / 1000000L);
    uint32_t timeSeconds = (timeNow - timeOnBoot);
	uint32_t h= timeSeconds / (60 * 60);
	uint32_t m= timeSeconds / 60 -  h * 60;
	uint32_t s= timeSeconds - h * 60 * 60 - m *60;
	uint32_t d= h / 24;
	h= h - d * 24;
	sprintf(uptime_str, "%lu-%02lu:%02lu:%02lu", d, h, m, s);
	return uptime_str;
} // upTime()

/**
---------------------------------------------------------------------------------------------------
		
								   CONSOLE INFO

---------------------------------------------------------------------------------------------------
**/
void PROJECTInfo(void)
{
	fprintf(stdout, "\n");
	fprintf(stdout, "------------------------------------------------------------\n");
	fprintf(stdout, "   SW\n");
	fprintf(stdout, "   %s\n", PROJECT_NAME);
	fprintf(stdout, "   %s\n", PROJECT_LOCATION);
	fprintf(stdout, "   %s\n", PROJECT_VERSION);
	fprintf(stdout, "   configMAX_PRIORITIES = %d\n", configMAX_PRIORITIES);
} // PROJECTInfo()

void SystemInfo(void)
{
	fprintf(stdout, "\n");
	fprintf(stdout, "------------------------------------------------------------\n");
	fprintf(stdout, "   SYSTEM\n");
	// Print chip information
	esp_chip_info_t chip_info;
	uint32_t flash_size;
	esp_chip_info(&chip_info);
	fprintf(stdout, "   This is %s chip with %d CPU core(s), %s%s%s%s\n",
		   CONFIG_IDF_TARGET,
		   chip_info.cores,
		   (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
		   (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
		   (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
		   (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

	unsigned major_rev = chip_info.revision / 100;
	unsigned minor_rev = chip_info.revision % 100;
	fprintf(stdout, "   silicon revision v%d.%d\n", major_rev, minor_rev);
	if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
		fprintf(stdout, "Get flash size failed\n");
		return;
	}

	fprintf(stdout, "   %" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
		   (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

	fprintf(stdout, "   Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
	fflush(stdout);
} // SystemInfo()

/**
---------------------------------------------------------------------------------------------------
		
								   Network Callbacks

---------------------------------------------------------------------------------------------------
**/
char IPaddr[32];
char MACaddr[32];

typedef struct Network_status_s
{
	int WiFi_lost;
	int TCP_lost;
} Network_status_type;

Network_status_type Network_status;

int WiFiCallback (int status, void *d)
{
	switch(status)
	{
		// ----------------------------------------------------------------
		// WIFI
		case NETWORK_STATUS_WIFI_CONNECTED:
			{	
				cstr_copy(IPaddr, (char*)d, sizeof(IPaddr));
				ESP_LOGI(TAG,"WIFI CONNECTED - IP address %s", IPaddr);
			}
			break;
		case NETWORK_STATUS_WIFI_DISCONNECTED:
			{		
				ESP_LOGE(TAG,"WIFI DISCONNECTED");
			}
			break;			
		case NETWORK_STATUS_WIFI_LOST:
			{				
				Network_status.WiFi_lost ++;
				ESP_LOGE(TAG,"WIFI LOST");
			}
			break;
		default:
			fprintf(stdout, "[WiFiCallback] ERROR DEFAULT\n");	
			fflush(stdout);	
	}
	return 0;
} // WiFiCallback()

int TCPCallback (int status, void *d)
{
	switch(status)
	{
		case NETWORK_STATUS_CLIENT_CONNECTED:
			{
				ESP_LOGI(TAG,"CLIENT CONNECTED");			
			}
			break;
		case NETWORK_STATUS_CLIENT_CONNECTION_CLOSED:
			{
				Network_status.TCP_lost ++;
				ESP_LOGE(TAG,"CLIENT CONNECTION CLOSED");
			}
			break;
		default:
			fprintf(stdout, "[TCPCallback] ERROR DEFAULT\n");	
			fflush(stdout);	
	}
	return 0;
} // TCPCallback()

/**
---------------------------------------------------------------------------------------------------
		
								   PUBLISH Callback

---------------------------------------------------------------------------------------------------
**/
int PUBLISHCallback (char *payload)
{
	char device[32];	
	char value[32];	
	int payload_length= strlen(payload);
	
	fprintf(stdout, "\nPUBLISHCallback %s", payload);	
	fflush(stdout);
	
	jsonParseValue("device", payload, 0, payload_length, device, sizeof(device));
	cstr_replace(device,'"','\0');
	
	jsonParseValue("status", payload, 0, payload_length, value, sizeof(value));
	cstr_replace(value,'"','\0');
	if(strlen(value))
	{
		fprintf(stdout, "\nPUBLISH status %s", payload);	
		fflush(stdout);			
	}
	jsonParseValue("set", payload, 0, payload_length, value, sizeof(value));
	cstr_replace(value,'"','\0');
	if(strlen(value))
	{
		if(strcmp(device, "led")==0)
		{
			led_is_on = (strcmp(value, "on")==0);
			gpio_set_level(PIN_GPIO_ONBOARD_LED, led_is_on? 1:0);
			char mqttpayload[64];
			snprintf(mqttpayload, sizeof(mqttpayload), "{\"device\":\"%s\",\"status\":\"%s\"}", device, led_is_on?  "on":"off");
			if(MQTT_is_connected()) mqtt_publish(network_tcp_send, DEVICE_MQTT_NAME"/info", mqttpayload);
			// MQTT_publish(mqttpayload);			
		}		
		else if(strcmp(device, "uno")==0)
		{
			
			relay_UNO_is_on = (strcmp(value, "on")==0);
			gpio_set_level(PIN_GPIO_RELAY_1, relay_UNO_is_on? 1:0);
			char mqttpayload[64];
			snprintf(mqttpayload, sizeof(mqttpayload), "{\"device\":\"%s\",\"status\":\"%s\"}", device, relay_UNO_is_on?  "on":"off");
			if(MQTT_is_connected()) mqtt_publish(network_tcp_send, DEVICE_MQTT_NAME"/info", mqttpayload);
			// MQTT_publish(mqttpayload);
		}
		else if(strcmp(device, "dos")==0)
		{
			relay_DOS_is_on = (strcmp(value, "on")==0);
			gpio_set_level(PIN_GPIO_RELAY_2, relay_DOS_is_on? 1:0);
			char mqttpayload[64];
			snprintf(mqttpayload, sizeof(mqttpayload), "{\"device\":\"%s\",\"status\":\"%s\"}", device, relay_DOS_is_on?  "on":"off");
			if(MQTT_is_connected()) mqtt_publish(network_tcp_send, DEVICE_MQTT_NAME"/info", mqttpayload);	
			// MQTT_publish(mqttpayload);			
		}			
		else
		{
			fprintf(stdout, "\nPUBLISH ERROR UNKNOWN DEVICE %s", payload);	
			fflush(stdout);	
		}
	}
	return 0;
} // PUBLISHCallback()

/**
---------------------------------------------------------------------------------------------------
		
								   Rest API Responses Callback

---------------------------------------------------------------------------------------------------
**/
// request comes in the pay load of the HTTP message and 
// is a json message with "device" and "key"
// {"device":"....","set":"...","key":"..."}'
// device= led / uno= relay 1 (outttermost) / dos= relay 2
// set= on / off
// key= shared key (for security)

int RestAPICallback(char * payload, char *response, size_t sz_response)
{
	char device[32];
	char info[32];
	char value[32];	
	int payload_length= strlen(payload);
	jsonParseValue("device", payload, 0, payload_length, device, sizeof(device));
	cstr_replace(device,'"','\0');
	jsonParseValue("info", payload, 0, payload_length, info, sizeof(info));
	cstr_replace(info,'"','\0');
	
	if(strlen(device))
	{
		if(strcmp(device, "led")==0)
		{
			jsonParseValue("set", payload, 0, payload_length, value, sizeof(value));
			cstr_replace(value,'"','\0');
			led_is_on = (strcmp(value, "on")==0);
			gpio_set_level(PIN_GPIO_ONBOARD_LED, led_is_on? 1:0);
			snprintf(response, sz_response, "{\"device\":\"%s\",\"status\":\"%s\"}", device, led_is_on?  "on":"off");
			if(MQTT_is_connected()) mqtt_publish(network_tcp_send, DEVICE_MQTT_NAME"/info", response);
			// MQTT_publish(response);
		}	
		else if(strcmp(device, "uno")==0)
		{
			jsonParseValue("set", payload, 0, payload_length, value, sizeof(value));
			cstr_replace(value,'"','\0');
			relay_UNO_is_on = (strcmp(value, "on")==0);
			gpio_set_level(PIN_GPIO_RELAY_1, relay_UNO_is_on? 1:0);
			snprintf(response, sz_response, "{\"device\":\"%s\",\"status\":\"%s\"}", device, relay_UNO_is_on?  "on":"off");
			if(MQTT_is_connected()) mqtt_publish(network_tcp_send, DEVICE_MQTT_NAME"/info", response);
			// MQTT_publish(response);
		}
		else if(strcmp(device, "dos")==0)
		{
			jsonParseValue("set", payload, 0, payload_length, value, sizeof(value));
			cstr_replace(value,'"','\0');
			relay_DOS_is_on = (strcmp(value, "on")==0);
			gpio_set_level(PIN_GPIO_RELAY_2, relay_DOS_is_on? 1:0);
			snprintf(response, sz_response, "{\"device\":\"%s\",\"status\":\"%s\"}", device, relay_DOS_is_on?  "on":"off");
			if(MQTT_is_connected()) mqtt_publish(network_tcp_send, DEVICE_MQTT_NAME"/info", response);
			// MQTT_publish(response);
		}		
		else
		{
			snprintf(response, sz_response, "{\"device\":\"%s\",\"result\":\"%s\"}", device, "error");
		}
	}
	else if(strlen(info))
	{
		char uptime_str[64];
		snprintf(response, sz_response, "{\"uptime\":\"%s\", \"uno\":\"%s\",\"dos\":\"%s\", \"led\":\"%s\"}", 
			upTime(uptime_str), relay_UNO_is_on?"on":"off", relay_DOS_is_on?"on":"off", led_is_on?"on":"off");
	}
	else
	{
		snprintf(response, sz_response, "{\"result\":\"%s\"}", "error");
	}	
	return 0;
} // RestAPICallback


/**
---------------------------------------------------------------------------------------------------
		
								   GPIO INPUT (BOTTON)

---------------------------------------------------------------------------------------------------
**/
void callbackPressButtonAction(int igpio_status)
{
	printf("\nCALLBACK %s\n", igpio_status?"---":"PRESSED"); 
	if(igpio_status == 0)
	{
		led_is_on = !led_is_on;
		gpio_set_level(PIN_GPIO_ONBOARD_LED, led_is_on? 1:0);
	}
} // callbackPressButtonAction()


/**
---------------------------------------------------------------------------------------------------
		
								   MAIN

---------------------------------------------------------------------------------------------------
**/
void app_main(void)
{
	ESP_LOGI(TAG, "app_main");
	
	// IP address
	IPaddr[0]='\0';
	// MAC address
    unsigned char mac_base[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(mac_base, ESP_MAC_WIFI_STA));
	snprintf(MACaddr, sizeof(MACaddr), "%02X:%02X:%02X:%02X:%02X:%02X", mac_base[0],mac_base[1],mac_base[2],mac_base[3],mac_base[4],mac_base[5]);
	
	// Network statistics
	memset(&Network_status,0, sizeof(Network_status));
	
	// Console
	// print SW project info
	PROJECTInfo();
	// print System info
 	SystemInfo();
	fprintf(stdout, "   Free heap size: %"PRIu32"\n", esp_get_free_heap_size());
	
 	// --------------------------------------------------------------------------------------------
	// WIFI
	// Initialize NVS
	// NVS is needed for the WiFi library	
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);	
	// Connect WiFi
	network_wifi_init(WiFiCallback);
		
	// --------------------------------------------------------------------------------------------
	// TASKs MQTT & TCP
	// MQTT Mosquitto client	
	MQTT_client_create(TCPCallback, PUBLISHCallback, 2);
	
	// --------------------------------------------------------------------------------------------
	// TASK RestAPI
	// REST API SERVER
	network_server_create(RestAPICallback, 3);

	// --------------------------------------------------------------------------------------------
	// GPIO init
	gpiolib_output_init();
	// Set OUTPUT
	led_is_on= false;
	gpio_set_level(PIN_GPIO_ONBOARD_LED, 0);	// OFF
	relay_UNO_is_on= false;
	gpio_set_level(PIN_GPIO_RELAY_1, 0);		// OFF
	relay_DOS_is_on= false;
	gpio_set_level(PIN_GPIO_RELAY_2, 0);		// OFF
	// TASK gpio_task
	// On-board button GPIO input 
	gpiolib_input_init(callbackPressButtonAction, 2);
	
	// Create ESP Timer
	ESP_ERROR_CHECK(esp_timer_early_init());
	timeOnBoot = (uint32_t) (esp_timer_get_time() / 1000000L);
	
	while(1)
	{
		int c= getchar();
		if(c>0)
		{
			// Ctrl + a
			if(c==0x01)
			{
				char uptime_str[64];
				fprintf(stdout, "\n\n");
				fprintf(stdout, "------------------------------------------------------------\n");
				fprintf(stdout,"   UP time  %s\n", upTime(uptime_str));
				fprintf(stdout,"   IP       %s\n", IPaddr);
				fprintf(stdout,"   MAC      %s\n", MACaddr);
				fprintf(stdout,"   WiFi     %s\n", network_wifi_is_connected()?"OK":"FAIL");
				fprintf(stdout,"   TCP      %s\n", network_tcp_is_connected()?"OK":"FAIL");
				fprintf(stdout,"   NQTT     %s\n", MQTT_is_connected()?"OK":"FAIL");

				PROJECTInfo();
				SystemInfo();
				fflush(stdout);
				gpio_dump_io_configuration(stdout, (1ULL << PIN_GPIO_RELAY_1) | (1ULL << PIN_GPIO_RELAY_2) | (1ULL << PIN_GPIO_ONBOARD_LED));
			}
			// Ctrl + b
			else if(c==0x02)
			{
				led_is_on = !led_is_on;
				fprintf(stdout, "\nLed set %s", led_is_on?"ON":"OFF");
				fflush(stdout);
				gpio_set_level(PIN_GPIO_ONBOARD_LED, led_is_on? 1:0);
				char mqttpayload[64];
				snprintf(mqttpayload, sizeof(mqttpayload), "{\"device\":\"led\",\"status\":\"%s\"}", led_is_on?  "on":"off");
				MQTT_publish(mqttpayload);
				//if(MQTT_is_connected()) mqtt_publish(network_tcp_send, DEVICE_MQTT_NAME"/info", mess);					
			}	
			// Ctrl + z
			else if(c==0x1a)
			{
				relay_UNO_is_on = !relay_UNO_is_on;
				fprintf(stdout, "\nRELAY 1 %s", relay_UNO_is_on?"ON":"OFF");
				fflush(stdout);
				gpio_set_level(PIN_GPIO_RELAY_1, relay_UNO_is_on? 1:0);
				char mqttpayload[64];
				snprintf(mqttpayload, sizeof(mqttpayload), "{\"device\":\"uno\",\"status\":\"%s\"}", relay_UNO_is_on?  "on":"off");
				MQTT_publish(mqttpayload);
				//if(MQTT_is_connected()) mqtt_publish(network_tcp_send, DEVICE_MQTT_NAME"/info", mess);	
			}			
			// Ctrl + x
			else if(c==0x18)
			{
				relay_DOS_is_on = !relay_DOS_is_on;
				fprintf(stdout, "\nRELAY 2 %s", relay_DOS_is_on?"ON":"OFF");
				fflush(stdout);
				gpio_set_level(PIN_GPIO_RELAY_2, relay_DOS_is_on? 1:0);
				char mqttpayload[64];
				snprintf(mqttpayload, sizeof(mqttpayload), "{\"device\":\"dos\",\"status\":\"%s\"}", relay_DOS_is_on?  "on":"off");
				MQTT_publish(mqttpayload);
				//if(MQTT_is_connected()) mqtt_publish(network_tcp_send, DEVICE_MQTT_NAME"/info", mess);					
			}			
			else {
				fprintf(stdout, "\ncommmand is %c (0x%x)\n", c, c);
				fflush(stdout);
			}
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
} // app_main

// END OF FILE