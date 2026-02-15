#ifndef _NETWORKLIB_H_
#define _NETWORKLIB_H_

#define NETWORK_STATUS_CLIENT_CONNECTED				0x10
#define NETWORK_STATUS_CLIENT_CONNECTION_CLOSED		0x11

void network_tcp_init(int (*callback) (int,void*));
bool network_tcp_is_connected(void);
int network_tcp_connect(void);
int network_tcp_send(char *message, size_t n);
int network_tcp_receive(char *message, size_t message_sz);
void network_tcp_close(void);

#endif
// END OF FILE