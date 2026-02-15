#ifndef _MQTT_H_
#define _MQTT_H_

int mqtt_connect(int(*f)(char*,size_t));
int mqtt_publish(int(*f)(char*,size_t), const char *topic, const char *message);
int mqtt_subscribe(int(*f)(char*,size_t), uint16_t id, const char *topic);
int mqtt_unsubscribe(int(*f)(char*,size_t), uint16_t id, const char *topic);
int mqtt_ping(int (*f) (char*, size_t ));
int mqtt_disconnect(int(*f)(char*,size_t));

int mqtt_payload(char *Control_Packet_type, char *data, uint8_t n, char* topic, char *payload, size_t max_sz);
uint8_t mqtt_packet_length(char *data, uint8_t n);
int mqtt_decode(char*, char*, uint8_t );



#define THE_LOWEST_OF(a,b) (a<b?a:b)

// MQTT Control Packet type 
// Position: byte 1, bits 7-4
						// Direction of flow 						Description					
#define CONNECT 	1 	// Client to Server 						Client request to connect to Server 
#define CONNACK 	2 	// Server to Client 						Connect acknowledgment 
#define PUBLISH 	3 	// Client to Server or Server to Client 	Publish message
#define PUBACK 		4 	// Client to Server or Server to Client  	Publish acknowledgment 
#define PUBREC 		5 	// Client to Server or Server to Client		Publish received (assured delivery part 1) 
#define PUBREL 		6 	// Client to Server or Server to Client  	Publish release (assured delivery part 2) 
#define PUBCOMP 	7 	// Client to Server or Server to Client  	Publish complete (assured delivery part 3) 
#define SUBSCRIBE 	8 	// Client to Server 						Client subscribe request 
#define SUBACK 		9 	// Server to Client 						Subscribe acknowledgment 
#define UNSUBSCRIBE 10 	// Client to Server 						Unsubscribe request 
#define UNSUBACK 	11 	// Server to Client 						Unsubscribe acknowledgment 
#define PINGREQ 	12 	// Client to Server 						PING request 
#define PINGRESP 	13 	// Server to Client 						PING response 
#define DISCONNECT 	14 	// Client to Server 						Client is disconnecting 
#define Reserved 	15 	// Forbidden Reserved 


/**
---------------------------------------------------------------------------------------------------
	CONNECT	
---------------------------------------------------------------------------------------------------
**/

//  Connect Flags 
#define CONNECT_FLAG_USER_NAME 		0b01000000
#define CONNECT_FLAG_PASSWORD 		0b00100000
#define CONNECT_FLAG_WILL_RETAIN 	0b00010000
#define CONNECT_FLAG_WILL_QOS 		0b00001000
#define CONNECT_FLAG_WILL_FLAG 		0b00000100
#define CONNECT_FLAG_CLEAN_SESSION	0b00000010	// Client and Server can store Session state to enable reliable messaging to continue across a sequence of Network Connections.

//  1  2  1  2  3  4  5  6  7  8  9 10 11 12
// 10 0C 00 04 4D 51 54 54 04 02 00 3C 00 00          ....MQTT...<..
#define MQTT_CONNECT_DEFAULT_MESSAGE() 							\
    {                         									\
        .Control_Packet_type =   ((CONNECT << 4) & 0xF0), 		\
        .Remaining_Length =      0x0C,  						\
        .Protocol_Name_Length =  ENDIAN(0x04),   				\
        .Protocol_Name =         { 'M', 'Q', 'T', 'T', }, 		\
        .Protocol_Level =        4,  					    	\
        .Connect_Flags =         CONNECT_FLAG_CLEAN_SESSION, 	\
		.Keep_Alive =        	 ENDIAN(0x3C),  		    	\
        .Payload =               0x0000  					   	\
     }
	 
typedef struct mqtt_connect_s
{
	// CONNECT Packet 
	// (1) fixed header 
	uint8_t Control_Packet_type;			// byte 1
	uint8_t Remaining_Length;				// byte 2
	// (2) Variable header 
	// 		Protocol Name 
	uint16_t Protocol_Name_Length;			// byte 1..2 Length MSB 
	uint8_t Protocol_Name[4];				// byte 3..6
	uint8_t Protocol_Level;					// byte 7 Protocol Version byte 
	uint8_t Connect_Flags;					// byte 8 
	// 		Keep Alive 
	uint16_t Keep_Alive;					// byte 9 MSB .. 10 LSB 
	// (3) Payload
	uint16_t Payload;						// The Client Identifier (ClientId) MUST be present and MUST be the first field in the CONNECT packet payload
} mqtt_connect_message_type;


/**
---------------------------------------------------------------------------------------------------
	PUBLISH	
---------------------------------------------------------------------------------------------------
**/
//  1  2  1  2  3  4  5  6  7  8  9 10 11 12
// 30 6A 00 15 7A 69 67 62 65 65 32 6D 71 74 74 2F   
#define MQTT_PUBLISH_DEFAULT_MESSAGE() 							\
    {                         									\
        .Control_Packet_type =   ((PUBLISH << 4) & 0xF0), 		\
        .Remaining_Length =      0x00,  						\
        .Topic_Name_Length =     0,        						\
     }
#define PUBLISH_VARIABLE_SIZE	252	 
typedef struct mqtt_publish_s
{
	//  PINGREQ Packet fixed header 
	uint8_t Control_Packet_type;			// byte 1
	uint8_t Remaining_Length;				// byte 2
	// Variable header 
	// 		Topic Name 
	// 		Packet Identifier - only present in PUBLISH Packets where the QoS level is 1 or 2.
	uint16_t Topic_Name_Length;				// bytes 1..2
	// Payload
	//		The Payload contains the Application Message that is being published
	uint8_t variable[PUBLISH_VARIABLE_SIZE]; // topic & message
} mqtt_publish_message_type;


/**
---------------------------------------------------------------------------------------------------
	PINGREQ	
---------------------------------------------------------------------------------------------------
**/
//  1  2  1  2  3  4  5  6  7  8  9 10 11 12
// D0 00 
#define MQTT_PINGREQ_DEFAULT_MESSAGE() 							\
    {                         									\
        .Control_Packet_type =   ((PINGREQ << 4) & 0xF0), 		\
        .Remaining_Length =      0x00,  						\
     }
	 
typedef struct mqtt_pingreq_s
{
	//  PINGREQ Packet fixed header 
	uint8_t Control_Packet_type;			// byte 1
	uint8_t Remaining_Length;				// byte 2
	// Variable header 
	// The PINGREQ Packet has no variable header. 
	// Payload
	// The PINGREQ Packet has no payload. 
} mqtt_pingreq_message_type;


/**
---------------------------------------------------------------------------------------------------
	DISCONNECT	
---------------------------------------------------------------------------------------------------
**/
//  1  2  1  2  3  4  5  6  7  8  9 10 11 12
// E0 00 
#define MQTT_DISCONNECT_DEFAULT_MESSAGE() 						\
    {                         									\
        .Control_Packet_type =   ((DISCONNECT << 4) & 0xF0), 	\
        .Remaining_Length =      0x00,  						\
     }
	 
typedef struct mqtt_disconnect_s
{
	//  DISCONNECT Packet fixed header 
	uint8_t Control_Packet_type;			// byte 1
	uint8_t Remaining_Length;				// byte 2
	// Variable header 
	// The DISCONNECT Packet has no variable header. 
	// Payload
	// The DISCONNECT Packet has no payload. 
} mqtt_disconnect_message_type;


/**
---------------------------------------------------------------------------------------------------
	SUBSCRIBE	
---------------------------------------------------------------------------------------------------
**/
//  1  2  1  2  3  4  5  
// 82 1A 00 01 00 15 7A 69 67 62 65 65 32 6D 71 74 74 2F 49 41 4D 53 45 4E 53 4F 52 00        
#define MQTT_SUBSCRIBE_DEFAULT_MESSAGE() 								\
    {                         											\
        .Control_Packet_type =   (((SUBSCRIBE << 4) & 0xF0) | 0x02), 	\
        .Remaining_Length =      0x00,  								\
        .Packet_Identifier =     0,  									\
        .Topic_Filter_Length =   0,  									\
    }
#define SUBSCRIBE_TOPIC_FILTER_SIZE	128	
typedef struct mqtt_subscribe_s
{
	//  DISCONNECT Packet fixed header 
	uint8_t Control_Packet_type;			// byte 1
	uint8_t Remaining_Length;				// byte 2
	// Variable header 
	// Packet Identifier 
	uint16_t Packet_Identifier;				// bytes 1..2
	// Payload
	uint16_t Topic_Filter_Length;			// bytes 1..2
	uint8_t Topic_Filter[SUBSCRIBE_TOPIC_FILTER_SIZE];				// bytes 3..N 
//	uint8_t Requested_QoS; 					// byte N+1
} mqtt_subscribe_message_type;



/**
---------------------------------------------------------------------------------------------------
	UNSUBSCRIBE	
---------------------------------------------------------------------------------------------------
**/
//  1  2  1  2  3  4  5  
//         
#define MQTT_UNSUBSCRIBE_DEFAULT_MESSAGE() 								\
    {                         											\
        .Control_Packet_type =   (((UNSUBSCRIBE << 4) & 0xF0) | 0x02), 	\
        .Remaining_Length =      0x00,  								\
        .Packet_Identifier =     0,  									\
        .Topic_Filter_Length =   0,  									\
    }
#define UNSUBSCRIBE_TOPIC_FILTER_SIZE	128	
typedef struct mqtt_unsubscribe_s
{
	//  DISCONNECT Packet fixed header 
	uint8_t Control_Packet_type;			// byte 1
	uint8_t Remaining_Length;				// byte 2
	// Variable header 
	// Packet Identifier 
	uint16_t Packet_Identifier;				// bytes 1..2
	// Payload
	uint16_t Topic_Filter_Length;			// bytes 1..2
	uint8_t Topic_Filter[UNSUBSCRIBE_TOPIC_FILTER_SIZE];				// bytes 3..N 
} mqtt_unsubscribe_message_type;



#endif
// END OF FILE