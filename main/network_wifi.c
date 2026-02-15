/** ************************************************************************************************
 *	WiFi library
 *  (c) Fernando R (iambobot.com)
 *
 *  1.0.0 - Dec 2025
 *
 ** ************************************************************************************************
**/

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <errno.h>

#include "config.h"
#include "network_wifi.h"

static const char *TAG = "WIFI";


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
#define ESP_WIFI_SSID      					WIFI_SSID
#define ESP_WIFI_PASS      					WIFI_PASSWORD
#define ESP_WIFI_MAXIMUM_RETRY  			5
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD 	WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SAE_MODE 					WPA3_SAE_PWE_BOTH
#define ESP_WIFI_H2E_IDENTIFIER 			""


/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
#define	WIFI_WAIT_TO_RETRY_AFTER_LOST	10

int network_wifi_lost_count= 0;

void WiFiWatchDog_task(void *pvParameter)
{
	network_wifi_lost_count= 0;
	vTaskDelay( 5000 / portTICK_PERIOD_MS );
	printf( "\nWatchDog started");

	while(1) 
	{
		if (network_wifi_lost_count)
		{
			if(--network_wifi_lost_count == 0)
			{
				fprintf(stdout, "\nWiFi try to reconnect\n");
				fflush(stdout);
				network_wifi_connect();	
			}		
		}
		// wait 5 seconds to repeat
		vTaskDelay( 5000 / portTICK_PERIOD_MS );
	}
} // WiFiWatchDog_task




//  WiFi
// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0; // attempts to connect WiFi before giving up
static int (*ConnectionStatusCallback) (int, void*)= 0;
static bool network_wifi_status_connected;



bool network_wifi_is_connected(void)
{
	return network_wifi_status_connected;
} // network_wifi_is_connected()

void network_wifi_connect()
{
	ESP_LOGI(TAG, "WiFi connect ...");
	s_retry_num= 0;
	esp_wifi_connect();
} // network_wifi_connect


static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	// printf("\n[WiFi event_handler] event_base= %s ", event_base);
	// if(event_base == WIFI_EVENT) printf("WIFI_EVENT"); 
	// else if(event_base == IP_EVENT) printf("IP_EVENT"); 
	// else  printf("other EVENT"); 
	// printf("\n");
	
    if (event_base == WIFI_EVENT)
	{		
		if(event_id == WIFI_EVENT_STA_START) 
		{
			esp_wifi_connect();
		} 
		else if (event_id == WIFI_EVENT_STA_DISCONNECTED) 
		{
			network_wifi_status_connected= false;
			if(ConnectionStatusCallback) ConnectionStatusCallback (NETWORK_STATUS_WIFI_DISCONNECTED,0);
			if (s_retry_num < ESP_WIFI_MAXIMUM_RETRY) 
			{
				esp_wifi_connect();
				s_retry_num++;
				ESP_LOGI(TAG, "retry to connect to the AP");
			} else 
			{
				xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
				// STATUS
				network_wifi_lost_count= WIFI_WAIT_TO_RETRY_AFTER_LOST;
				if(ConnectionStatusCallback) ConnectionStatusCallback (NETWORK_STATUS_WIFI_LOST,0);
			}
			ESP_LOGI(TAG,"connect to the AP fail");
		} 
	}
	
	// Connected
	else if (event_base == IP_EVENT)
	{
		if(event_id == IP_EVENT_STA_GOT_IP) 
		{
			ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
			ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
			s_retry_num = 0;
			xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
			// STATUS
			network_wifi_lost_count= 0;	
			
			// IP address is known
			char IPaddr[32];
			snprintf(IPaddr, sizeof(IPaddr), IPSTR, IP2STR(&event->ip_info.ip));
			//ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->ip_info.ip));
			network_wifi_status_connected= true;
			if(ConnectionStatusCallback) ConnectionStatusCallback (NETWORK_STATUS_WIFI_CONNECTED, IPaddr);
		}
	}
} // event_handler()

void network_wifi_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
	
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 	 &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,	IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
             // * Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             // * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             // * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             // * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = ESP_WIFI_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    // * Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
    // * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) 
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    // * xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   //  * happened. 
    if (bits & WIFI_CONNECTED_BIT) 
	{
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
    } 
	else if (bits & WIFI_FAIL_BIT) 
	{
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else 
	{
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
} // wifi_init_sta()



void network_wifi_init(int (*callbak) (int,void*))
{
	s_retry_num = 0;
	ConnectionStatusCallback= callbak;
	network_wifi_status_connected= false;
	
	fprintf(stdout,  "\nConnecting WiFi ...\n");
	network_wifi_sta();
	// TASK - WatchDog
	xTaskCreate( &WiFiWatchDog_task, "WiFiWatchDog_task", 2048, NULL, 1, NULL );
} // network_wifi_init



// END OF FILE