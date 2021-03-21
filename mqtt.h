#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "uart0.h"
#include "eth0.h"



//MQTT Control Header Flags
#define CONNECT     1
#define CONNACK     2
#define PUBLISH     3
#define PUBACK      4
#define SUBSCRIBE   8
#define UNSUBSCRIBE 10
#define DISCONNECT  14


//MQTT Packet
//-> Control Header (1 Byte)
//    -> Packet Type (4bits)
//    -> Flags (4bits)
//-> Packet Rem. Length (1-4 bytes)
//-> Variable length header (>= 0)
//-> Payload(>=0 bytes)

typedef struct MQTT_Packet
{
  uint8_t controlHeader;
  uint8_t remLength[4];
  uint8_t data[0];
} mqttPack;



typedef struct _connectHeader
{
    uint16_t protocolNameLength;
    uint8_t  protocolName[4];
    uint8_t protocolLevel;
    uint8_t connectFlag;
    uint16_t keepAliveSeconds;
    uint8_t data[0];
}connectHeader;

typedef struct _connackHeader
{
    uint8_t ackFlags;
    uint8_t returnCode;
}connackHeader;

void getutf8Encoding(char* outputStr, char* inputStr, uint16_t strlen);
void getMQTTPacket(uint8_t* tcpData, uint8_t type, uint8_t flags);
uint16_t getConnectPacket(uint8_t* tcpData, uint8_t protocolLevel, uint8_t connectFlags, uint16_t keepalive, char* clientID, uint16_t clientIDLength);
bool isMQTTconnack(uint8_t* packet);
uint32_t getRemLength(uint32_t input, uint8_t* fieldCount);


#endif
