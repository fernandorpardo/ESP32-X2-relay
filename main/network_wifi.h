#ifndef _NETWORK_WIFI_H_
#define _NETWORK_WIFI_H_

#define NETWORK_STATUS_WIFI_CONNECTED		1
#define NETWORK_STATUS_WIFI_DISCONNECTED	2
#define NETWORK_STATUS_WIFI_LOST			3

void network_wifi_init(int (*callback) (int, void*));
bool network_wifi_is_connected(void);
void network_wifi_connect();

#endif
// END OF FILE