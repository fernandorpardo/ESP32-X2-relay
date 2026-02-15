/** ************************************************************************************************
 *	MQTT library
 *  (c) Fernando R (iambobot.com)
 *
 * 	1.0.0 - October 2024 - created
 * 	1.1.0 - January 2025
 *		- Adapted to ESP IDF (plain C language)
 *      - int(*f)(char*,size_t)
 *
 ** ************************************************************************************************
**/

#include <stdio.h>		// fprintf (mqtt_decode())
#include <string.h>		// memcpy
#include "config.h"
#include "mqtt.h"
/**
---------------------------------------------------------------------------------------------------
		
								   CLIENT API

---------------------------------------------------------------------------------------------------
**/
int mqtt_connect(int(*f)(char*,size_t))
{
	mqtt_connect_message_type connect_m = MQTT_CONNECT_DEFAULT_MESSAGE();
	return f((char*)&connect_m, sizeof(mqtt_connect_message_type));
} // mqtt_connect()

int mqtt_publish(int(*f)(char*,size_t), const char *topic, const char *message)
{
	mqtt_publish_message_type publish_m = MQTT_PUBLISH_DEFAULT_MESSAGE();
	uint8_t topic_length= (uint8_t)strlen(topic);
	publish_m.Topic_Name_Length= ENDIAN((uint16_t)topic_length);
	uint8_t i=0;
// C++	for(uint8_t j=0; j<topic_length    && i<sizeof(mqtt_publish_message_type::variable); i++, j++) publish_m.variable[i]= topic[j];
	for(uint8_t j=0; j<topic_length    && i<PUBLISH_VARIABLE_SIZE; i++, j++) publish_m.variable[i]= topic[j];
// C++	for(uint8_t j=0; j<strlen(message) && i<sizeof(mqtt_publish_message_type::variable); i++, j++) publish_m.variable[i]= message[j];
	for(uint8_t j=0; j<strlen(message) && i<PUBLISH_VARIABLE_SIZE; i++, j++) publish_m.variable[i]= message[j];
	publish_m.Remaining_Length= 2 + i;
	uint8_t n= 2 + publish_m.Remaining_Length;
	return f((char*)&publish_m, n);
} // mqtt_publish()

int mqtt_ping(int(*f)(char*,size_t))
{
	mqtt_pingreq_message_type pingreq_m = MQTT_PINGREQ_DEFAULT_MESSAGE();
	return f((char*)&pingreq_m, sizeof(mqtt_pingreq_message_type));
} // mqtt_ping()

int mqtt_disconnect(int(*f)(char*,size_t))
{
	mqtt_disconnect_message_type disconnect_m = MQTT_DISCONNECT_DEFAULT_MESSAGE();
	return f((char*)&disconnect_m, sizeof(mqtt_disconnect_message_type));
} // mqtt_disconnect()

int mqtt_subscribe(int(*f)(char*,size_t), uint16_t id, const char *topic)
{
	mqtt_subscribe_message_type subscribe_m = MQTT_SUBSCRIBE_DEFAULT_MESSAGE();
	subscribe_m.Packet_Identifier= ENDIAN(id);
// C++	uint16_t topic_length= THE_LOWEST_OF(sizeof(mqtt_subscribe_message_type::Topic_Filter), strlen(topic));
	uint16_t topic_length= THE_LOWEST_OF(SUBSCRIBE_TOPIC_FILTER_SIZE, strlen(topic));
	memcpy(subscribe_m.Topic_Filter, topic, topic_length);
	subscribe_m.Topic_Filter_Length= ENDIAN(topic_length);
	subscribe_m.Remaining_Length= 2 + 2 + topic_length + 1;
	// Requested_QoS - last byte right after Topic_Filter
	//  QoS is 0,1 or 2 
	subscribe_m.Topic_Filter[topic_length]= 0x00;
	size_t n= 2 + (size_t)subscribe_m.Remaining_Length;
	return f((char*)&subscribe_m, n);
} // mqtt_subscribe()

int mqtt_unsubscribe(int(*f)(char*,size_t), uint16_t id, const char *topic)
{
	mqtt_unsubscribe_message_type unsubscribe_m = MQTT_UNSUBSCRIBE_DEFAULT_MESSAGE();
	unsubscribe_m.Packet_Identifier= ENDIAN(id);
// C++	uint16_t topic_length= THE_LOWEST_OF(sizeof(mqtt_subscribe_message_type::Topic_Filter), strlen(topic));
	uint16_t topic_length= THE_LOWEST_OF(UNSUBSCRIBE_TOPIC_FILTER_SIZE, strlen(topic));
	memcpy(unsubscribe_m.Topic_Filter, topic, topic_length);
	unsubscribe_m.Topic_Filter_Length= ENDIAN(topic_length);
	unsubscribe_m.Remaining_Length= 2 + 2 + topic_length;
	size_t n= 2 + (size_t)unsubscribe_m.Remaining_Length;
	return f((char*)&unsubscribe_m, n);
} // mqtt_unsubscribe()


/**
---------------------------------------------------------------------------------------------------
		
								   PROXY

---------------------------------------------------------------------------------------------------
**/
const char *Control_Packet_type_txt[]=
{
	"Reserved",
	"CONNECT","CONNACK","PUBLISH","PUBACK","PUBREC","PUBREL","PUBCOMP","SUBSCRIBE","SUBACK","UNSUBSCRIBE","UNSUBACK","PINGREQ","PINGRESP","DISCONNECT",
	"Reserved"
};

// Trace
int mqtt_decode(char *Control_Packet_type, char *data, uint8_t n)
{
	if(n<=0) return -1;
	// (1) fixed header  
	uint8_t type= (data[0] >> 4) & 0x0F;
	uint8_t Remaining_Length= data[1];
	fprintf(stdout, "\033[36m%s\033[0m", Control_Packet_type_txt[type]);
	fprintf(stdout, "\nremaining %d bytes", Remaining_Length);
	int bytes_of_next_packets= 0;
	*Control_Packet_type= type;
	switch(type)
	{
		case CONNECT:
			// 10 0C 00 04 4D 51 54 54 04 02 00 3C 00 00
			{
				fprintf(stdout, " (%s)", (n == 2 + Remaining_Length) ? "OK":"ERROR");				
				fprintf(stdout, "\nProtocol Version %d", data[8]);
			}
			break;
		case CONNACK:
			// 20 02 00 00 
			{
				uint8_t Connect_Return_code = data[3];
				fprintf(stdout, " (%s)", (n == 4) ? "OK":"ERROR");				
				fprintf(stdout, "\nConnect_Return_code (%d)", Connect_Return_code);
				switch(Connect_Return_code)
				{
					case 0:
						fprintf(stdout, " Connection Accepted");
						//if(!shd->proxy_mode) timeAdd( 5000, PINGREQ_time_callback );
						//*Control_Packet_type= CONNACK;
					break;
					case 1:
						fprintf(stdout, " Connection Refused, unacceptable protocol version");
					break;
					case 2:
						fprintf(stdout, " Connection Refused, identifier rejected");
					break;
					default:
						fprintf(stdout, " Connect_Return_code unknown");
				}
			}
			break;
		case PUBLISH:
			// PUBLISH Control Packet is sent to transport an Application Message
			// 30 69 00 15              7A 69 67 62 65 65 32 6D 71 74 74 2F
            // 30 A4 01 00 1E           7A 69 67 62 65 65 32 6D 71 74 74    0....zigbee2mqtt
			{
				char topic_str[256];
				char Application_Message[256];
				uint8_t wtf_byte= data[2];
				uint8_t packet_length=  2 + wtf_byte + data[1];
				// (2) Variable header 
				uint16_t topic_length= ENDIAN(*(uint16_t*)&data[2+wtf_byte]); 
				uint16_t i=0;
				for (; i<(uint16_t)topic_length && i<(uint16_t)(sizeof(topic_str)-1); i++) topic_str[i]= data[4 + wtf_byte + i];
				topic_str[i]='\0';		
				// (3) Payload
				uint16_t Application_Message_length= packet_length - (2 + wtf_byte + 2 + topic_length);
				uint16_t j=0;
				for (; j<Application_Message_length && j<sizeof(Application_Message)-1; j++, i++) Application_Message[j]= data[4 + wtf_byte + i];
				Application_Message[j]= '\0';
		
				// Flag bits
				// 		|   Fixed header flags   | Bit 3 | Bit 2 | Bit 1 | Bit 0  |
				// 		|   Used in MQTT 3.1.1   | DUP1  | QoS   | QoS   | RETAIN |
				fprintf(stdout, " (%s)", (n == 2 + wtf_byte + Remaining_Length) ? "OK":"ERROR");				
				fprintf(stdout, " DUP %b | QoS %2b | RETAIN %b", data[0] & 0x08, (data[0] & 0x06) >> 1, data[0] & 0x01);
				fprintf(stdout, "\ntopic (%d bytes): %s", topic_length, topic_str);
				fprintf(stdout, "\nApplication_Message (%d bytes):\n%s", Application_Message_length, Application_Message);
				fflush(stdout);	
				bytes_of_next_packets= n - (2 + wtf_byte + Remaining_Length);
			}
			break;
		case PUBACK:
			// A PUBACK Packet (Publish acknowledgement) is the response to a PUBLISH Packet with QoS level 1. 
			{
				uint16_t Packet_Identifier= 16 * (uint16_t)data[2] + (uint16_t)data[3];
				fprintf(stdout, " (%s)", (n == 2 + Remaining_Length) ? "OK":"ERROR");				
				fprintf(stdout, "\nPacket Identifier: %d", Packet_Identifier);
			}
			break;
		case PUBREC:
			// A PUBREC Packet (Publish received) is the response to a PUBLISH Packet with QoS 2. It is the second packet of the QoS 2 protocol exchange. 
			break;
		case PUBREL:
			// A PUBREL Packet (Publish release) is the response to a PUBREC Packet. It is the third packet of the QoS 2 protocol exchange. 
			break;
		case PUBCOMP:
			// The PUBCOMP Packet (Publish complete) is the response to a PUBREL Packet. It is the fourth and final packet of the QoS 2 protocol exchange. 
			break;
		case SUBSCRIBE:
			// The SUBSCRIBE Packet is sent to create one or more Subscriptions. Each Subscription registers a Client’s interest in one or more Topics
			// 82 1A 00 02 00 15 7A 69 67 62 65 65 32 6D 71 74 74 2F 49 41 4D 53 45 4E 53 4F 52 00 
			{
				char topic[256];
				uint16_t Packet_Identifier= 16* (uint16_t)data[2] + (uint16_t)data[3];
				uint16_t payload_length= 16* (uint16_t)data[4] + (uint16_t)data[5];
				int i=0;
				for (; i<(int)payload_length && i<(int)(sizeof(topic)-1); i++) topic[i]= data[6+i];
				topic[i]='\0';
				fprintf(stdout, " (%s)", (n == 6 + payload_length + 1) ? "OK":"ERROR");				
				fprintf(stdout, "\nPacket Identifier: %d", Packet_Identifier);
				fprintf(stdout, "\npayload_length: %d", payload_length);
				fprintf(stdout, "\ntopic: %s", topic);		
				bytes_of_next_packets= n - (6 + payload_length + 1);
			}
			break;
		case SUBACK:
			// A SUBACK Packet is sent by the Server to the Client to confirm receipt and processing of a SUBSCRIBE Packet. 
			// 90 03 00 08 00 
				fprintf(stdout, " (%s)", (n == 2 + Remaining_Length ) ? "OK":"ERROR");		
			break;
		case UNSUBSCRIBE:
			// An UNSUBSCRIBE Packet is sent by the Client to the Server, to unsubscribe from topics.
			// A2 19 00 01 00 15 7A 69 67 62 65 65 32 6D 71 74 74 2F 49 41 4D 53 45 4E 53 4F 52
			{
				char topic[256];
				uint16_t Packet_Identifier= 16* (uint16_t)data[2] + (uint16_t)data[3];
				uint16_t payload_length= 16* (uint16_t)data[4] + (uint16_t)data[5];
				int i=0;
				for (; i<(int)payload_length && i<(int)(sizeof(topic)); i++) topic[i]= data[6+i];
				topic[i]='\0';
				fprintf(stdout, " (%s)", (n == 6 + payload_length) ? "OK":"ERROR");				
				fprintf(stdout, "\nPacket Identifier: %d", Packet_Identifier);
				fprintf(stdout, "\npayload_length: %d", payload_length);
				fprintf(stdout, "\ntopic: %s", topic);		
				bytes_of_next_packets= n - (6 + payload_length + 1);
			}
			break;
		case UNSUBACK:
			// The UNSUBACK Packet is sent by the Server to the Client to confirm receipt of an UNSUBSCRIBE Packet.
			// B0 02 00 01 
				fprintf(stdout, " (%s)", (n == 2 + Remaining_Length ) ? "OK":"ERROR");		
			break;
		case PINGREQ:
			// The PINGREQ Packet is sent from a Client to the Server. It can be used to:   
			// 		1. Indicate to the Server that the Client is alive in the absence of any other Control Packets being sent from the Client to the Server.  
			// 		2. Request that the Server responds to confirm that it is alive.  
			// 		3. Exercise the network to indicate that the Network Connection is active. 
			// C0 00
				fprintf(stdout, " (%s)", (n == 2) ? "OK":"ERROR");	
			break;
		case PINGRESP:
			// A PINGRESP Packet is sent by the Server to the Client in response to a PINGREQ Packet. It indicates that the Server is alive.
				fprintf(stdout, " (%s)", (n == 2) ? "OK":"ERROR");	
			break;
		case DISCONNECT:
			// The DISCONNECT Packet is the final Control Packet sent from the Client to the Server. It indicates that the Client is disconnecting cleanly
			// E0 00 
				fprintf(stdout, " (%s)", (n == 2) ? "OK":"ERROR");	
			break;
		default:
		{
			fprintf(stdout, "\nmqtt_decode default");
		}
	}
	fflush(stdout);
	return bytes_of_next_packets;
} // mqtt_decode()


int mqtt_payload(char *Control_Packet_type, char *data, uint8_t n, char *topic, char *payload, size_t max_sz)
{
	if(n<=0) return -1;
	// (1) fixed header  
	uint8_t type= (data[0] >> 4) & 0x0F;
	*Control_Packet_type= type;
	topic[0]= '\0';
	payload[0]= '\0';
	int payload_length= 0;
	switch(type)
	{
		case CONNECT:
			break;
		case CONNACK:
			break;
		case PUBLISH:
			{
				char topic_str[128];
				char Application_Message_str[128];
				uint8_t wtf_byte= data[2];
				uint8_t packet_length=  2 + wtf_byte + data[1];
				// (2) Variable header 
				uint16_t topic_length= ENDIAN(*(uint16_t*)&data[2+wtf_byte]); 
				size_t i=0;
				for (i=0; i<(size_t)topic_length && i<(sizeof(topic_str)-1); i++) topic_str[i]= data[4 + wtf_byte + i];
				topic_str[i]='\0';		
				// (3) Payload
				uint16_t Application_Message_length= packet_length - (2 + wtf_byte + 2 + topic_length);
				uint16_t j=0;
				for (; j<Application_Message_length && j<sizeof(Application_Message_str)-1; j++, i++) Application_Message_str[j]= data[4 + wtf_byte + i];
				Application_Message_str[j]= '\0';
				// return values
				// (1) topic
				for(i=0; i<max_sz && topic_str[i]!='\0'; i++) topic[i]= topic_str[i];
				topic[i]='\0';
				// (2) payload
				for(i=0; i<max_sz && Application_Message_str[i]!='\0'; i++) payload[i]= Application_Message_str[i];
				payload[i]='\0';
			}
			break;
		case PUBACK:
			break;
		case PUBREC:
			break;
		case PUBREL:
			break;
		case PUBCOMP:
			break;
		case SUBSCRIBE:
			break;
		case SUBACK:
			break;
		case UNSUBSCRIBE:
			break;
		case UNSUBACK:
			break;
		case PINGREQ:
			break;
		case PINGRESP:
			break;
		case DISCONNECT:
			break;
		default:
		{
			
		}
	}
	return payload_length;
} // mqtt_payload()

uint8_t mqtt_packet_length(char *data, uint8_t n)
{
	uint8_t type= (data[0] >> 4) & 0x0F;
	uint8_t packet_length= 2 + data[1];
	switch(type)
	{
		case CONNECT:
			break;
		case CONNACK:
			break;
		case PUBLISH:
			{
				uint8_t wtf_byte= data[2];
				packet_length=  2 + wtf_byte + data[1];
			}
			break;
		case PUBACK:
			break;
		case PUBREC:
			break;
		case PUBREL:
			break;
		case PUBCOMP:
			break;
		case SUBSCRIBE:
			break;
		case SUBACK:
			break;
		case UNSUBSCRIBE:
			break;
		case UNSUBACK:
			break;
		case PINGREQ:
			break;
		case PINGRESP:
			break;
		case DISCONNECT:
			break;
		default:
			packet_length= 0;
	}
	return packet_length;
} // mqtt_decode()


// END OF FILE