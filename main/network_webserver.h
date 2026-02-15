#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

// If SERVER_PERMISSIVE is defined (uncomment below), the response is generated for any HTTP request regarless of the HTTP payload 
// it is meant for testing with a browser (http://<device's ip address>)
// #define	SERVER_PERMISSIVE

// Configuration parameters
#define SERVER_PORT 			80
#define SERVER_NAME		        "modbus2MQTT/1.0"
#define SERVER_CONTENT_TYPE     "application/json"


void network_server_create(int (*callback) (char*, char*, size_t), UBaseType_t);

#endif
// END OF FILE