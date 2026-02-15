/** ************************************************************************************************
 *	WEB server for REST API
 *  (c) Fernando R (iambobot.com)
 *
 * 	1.0.0 - December 2025 - created
 *
 ** ************************************************************************************************
**/
#include <stdio.h>
#include "esp_log.h"		// ESP_LOGW
#include "lwip/sockets.h"

#include "config.h"
#include "cstr.h"
#include "network_webserver.h"

static const char *TAG = "webserver";

/**
---------------------------------------------------------------------------------------------------
		
								   SERVER

---------------------------------------------------------------------------------------------------
**/
// https://github.com/espressif/esp-idf/tree/v5.5.1/examples/protocols/sockets/tcp_server


#define KEEPALIVE_IDLE              5
#define KEEPALIVE_INTERVAL          5
#define KEEPALIVE_COUNT             3

static int (*NetworkServerCallback) (char*,char*,size_t)= 0;
static char httpdata[1024];
static char response[512];

// Process payload and return response
// payload is a json message with "type" and "key"
// {"type":"....","key":"...", ....... }'
// key is a server/client shared string defined in .h file
int server_response()
{
	response[0]='\0';
#ifdef SERVER_PERMISSIVE
	if(NetworkServerCallback) NetworkServerCallback ((char*) 0, (char*)response, sz_response);
#else
	char value[32];		
	int i= cstr_find(httpdata, "\r\n\r\n", 0, 0);
	if(i<0 || !(strlen(&httpdata[i])>4) )
	{
		fprintf(stdout, "\n%s No HTML", TAG);
		return -1;
	}				
	// payload is a json message with "type" and "key"
	// {"type":"....","key":"...", ....... }'
	char *payload= &httpdata[i+4];
	int payload_length= strlen(payload);
	fprintf(stdout, "\npayload %s", payload);
	// (1) Check key
	jsonParseValue("key", payload, 0, payload_length, value, sizeof(value));
	cstr_replace(value,'"','\0');
	if(strcmp(value, SERVER_SECURE_KEY)!=0)
	{
		fprintf(stdout, "\nwrong key: %s", value);
		return -1;
	}
	fprintf(stdout, "\nkey: %s %s", value, SERVER_SECURE_KEY);
	// (2) type
	// jsonParseValue("type", payload, 0, payload_length, value, sizeof(value));
	// cstr_replace(value,'"','\0');
	
	if(NetworkServerCallback) NetworkServerCallback (payload, response, sizeof(response));
#endif
	return 0;
} // server_response

void tcp_server_task(void *pvParameters)
{
    char addr_str[32];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

    if (addr_family == AF_INET) {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(SERVER_PORT);
        ip_protocol = IPPROTO_IP;
    }

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Note that by default IPV6 binds to both protocols, it is must be disabled
    // if both protocols used at the same time (used in CI)
    setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));


    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", SERVER_PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) 
	{
        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        // Convert ip address to string
        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }

        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

		
	    int len = recv(sock, httpdata, sizeof(httpdata) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } 
		else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
        } 
		else 
		{
            httpdata[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG, "Received %d bytes", len);
			
			
			// fprintf(stdout,"\nReceived %d bytes", len);
			// cstr_dump(httpdata, len);
			// fprintf(stdout,"\n");
			// fflush(stdout);
			
			// ------------------------------------------------------------------------
			
			if( server_response() == 0)
			{
				snprintf(httpdata, sizeof(httpdata),
				"HTTP/1.1 200 OK\r\n"
				"Server: %s\r\n"					// SERVER_NAME
				"Content-Type: %s\r\n"				// SERVER_CONTENT_TYPE
				"Content-Length: %d\r\n"
				"Connection: close\r\n"
				"\r\n"
				"%s", 
				SERVER_NAME,
				SERVER_CONTENT_TYPE,
				strlen(response), 
				response);			
		
				int n = send(sock, httpdata, strlen(httpdata), 0);
				fprintf(stdout,"\nResponse sent %d bytes: %s", n, response);
			}
			else
			{
				fprintf(stdout,"\nNO response");
			}
			fprintf(stdout,"\n");
			fflush(stdout);
        }
		
        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void network_server_create(int (*callback) (char*, char*, size_t), UBaseType_t uxPriority)
{
	NetworkServerCallback= callback;
	xTaskCreate(tcp_server_task, "RestAPI", 4*1024, (void*)AF_INET, uxPriority, NULL);
}


// END OF FILE