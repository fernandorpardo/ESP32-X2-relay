/** ************************************************************************************************
 *	TCP client
 *  (c) Fernando R (iambobot.com)
 *
 *  1.0.0 - May 2025
 *
 ** ************************************************************************************************
**/

// https://github.com/espressif/esp-idf/blob/master/examples/protocols/sockets/tcp_client/main/tcp_client_v4.c

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>            // struct addrinfo
#include <arpa/inet.h>
#include "esp_netif.h"

#include "config.h"
#include "network_tcpclient.h"

static const char *TAG = "TCPCLIENT";
static int sockfd;


static int (*WEBClientCallback) (int,void*)= 0;
static bool network_tcp_status_connected;

void network_tcp_init(int (*callback) (int,void*))
{
	sockfd= -1;
	network_tcp_status_connected= false;
	WEBClientCallback= callback;
} // network_tcp_init()

bool network_tcp_is_connected(void)
{
	return network_tcp_status_connected;
} // network_tcp_is_connected()


char host_ip[] = MQTT_HOST_IP_ADDR;

int network_tcp_connect(void)
{
	sockfd =  socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (sockfd < 0) {
		ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
		return -1;
	}
	ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, MQTT_HOST_IP_PORT);
	// Set time-out before connect
	// RCV time-out milisec
	struct timeval timeout;      
	timeout.tv_sec = 0; 
	timeout.tv_usec = 500000;
	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		fprintf(stdout, "\n[network_tcp_connect] setsockopt() ERROR "); 
		perror("setsockopt() failed\n");
		fflush(stdout);
		close(sockfd);
		sockfd= -1;
		return -1;	
	}	
	// SND time-out seconds
	timeout.tv_sec = 1; 
	timeout.tv_usec = 0;
	if (setsockopt (sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		fprintf(stdout, "\n[network_tcp_connect] setsockopt() ERROR "); 
		perror("setsockopt() failed\n");
		fflush(stdout);
		close(sockfd);
		sockfd= -1;
		return -1;	
	}
	// connect
	struct sockaddr_in dest_addr;
	inet_pton(AF_INET, host_ip, &dest_addr.sin_addr);
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(MQTT_HOST_IP_PORT);
	int err = connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if (err != 0) {
		ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
		close(sockfd);
		sockfd= -1;
		return -1;
	}
	ESP_LOGI(TAG, "Successfully connected");
	
	network_tcp_status_connected= true;
	if(WEBClientCallback) WEBClientCallback (NETWORK_STATUS_CLIENT_CONNECTED,0);
	return 0;
} // network_tcp_connect()

void network_tcp_close(void)
{
	if (sockfd != -1) {
		ESP_LOGE(TAG, "Shutting down socket and restarting...");
		shutdown(sockfd, 0);
		close(sockfd);
		sockfd= -1;
		
		network_tcp_status_connected= false;
		if(WEBClientCallback) WEBClientCallback (NETWORK_STATUS_CLIENT_CONNECTION_CLOSED,0);
	}	
} // network_tcp_close()


int network_tcp_send(char *message, size_t len)
{
	fprintf(stdout,"\n[network_tcp_send] %d bytes ....", len);
	fflush(stdout);
	
	// ssize_t send(int sockfd, const void buf[.len], size_t len, int flags);
	ssize_t err = send(sockfd, message, len, 0);
	
	fprintf(stdout," DONE %s  %d", (len==err) ? "OK len=" : "*** ERROR *** err=", err);
	fflush(stdout);	
	
	if (err < 0) 
	{
		ESP_LOGE(TAG, "send failed: errno %d", errno);
		perror("send() failed\n");
		if(errno == 128)
		{
			printf("\n[network_tcp_send] TCP connection lost !!!\n"); 
			fflush(stdout);
			network_tcp_close();
		}
		return -1;
	}
	return 0;
} // network_tcp_send()


int network_tcp_receive(char *message, size_t max_sz)
{
	int n = recv(sockfd, message, max_sz, 0);
	// Error occurred during receiving
	if (n <= 0) {
		if(n == 0) 
		{
			printf("\n");
			ESP_LOGE(TAG, "[network_tcp_receive] DISCONNECTED\n");	
			network_tcp_close();
		}
		else if(errno == EAGAIN) // Resource temporarily unavailable		
		{
			// Not to worry, it is the time -out
			//fprintf(stdout,"+"); fflush(stdout);					
		}
		else if(errno == 128)
		{
			printf("\n");
			ESP_LOGE(TAG, "[network_tcp_receive] TCP connection lost !!!\n"); 
			network_tcp_close();
		}
		else
		{
			printf("\n");
			ESP_LOGE(TAG, "[network_tcp_receive] recv failed: errno %d", errno);
		}
		
		return -1;
	}
	// Data received	
	printf("\n");
	ESP_LOGI(TAG, "[network_tcp_receive] Received %d bytes from %s:", n, host_ip);
	
	return n;
} // network_tcp_receive




// END OF FILE