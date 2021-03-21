#include "mqtt.h"

void getutf8Encoding(char* outputStr, char* inputStr, uint16_t strlen)
{
    if(!(strlen > 0))
    {
        putsUart0("could not encode the given string. length should be greater than 0.\n\r");
        return;
    }

    *outputStr = (strlen >> 8) & 0xFF;
    outputStr++;
    *outputStr = strlen & 0xFF;
    outputStr++;

    while(*inputStr != 0)
    {
        *outputStr = *inputStr;
        outputStr++; inputStr++;
    }
    outputStr -= (strlen+2);
}

uint32_t getRemLength(uint32_t X, uint8_t* fieldCount)
{
    uint32_t encodedByte = 0;
    uint32_t temp = 0;
    do
    {
        temp = X % 128;
        X = X/128;
        if(X > 0) //if there are more data to encode, set the top bit of this byte
        {
            temp |= 128;
            (*fieldCount)++;
        }
        encodedByte = encodedByte << 8;
        encodedByte |= temp & 0xFF;
    }while (X>0);
    return encodedByte;
}

void getMQTTPacket(uint8_t* tcpData, uint8_t type, uint8_t flags)
{
    mqttPack* mqtt = (mqttPack*) tcpData;
    mqtt->controlHeader = (type << 4) + flags;
}


uint16_t getConnectPacket(uint8_t* tcpData, uint8_t protocolLevel, uint8_t connectFlags, uint16_t keepalive, char* clientID, uint16_t clientIDLength)
{
    mqttPack* mqtt = (mqttPack*) tcpData;

    uint32_t remlen;
    uint8_t fieldCount = 1;
    remlen = getRemLength((3 + sizeof(connectHeader) + clientIDLength), &fieldCount);

    mqtt->remLength[0] = 0;
    mqtt->remLength[1] = 0;
    mqtt->remLength[2] = 0;
    mqtt->remLength[3] = 0;

    uint8_t i;
    for(i = 0; i<fieldCount; i++)
    {
        mqtt->remLength[i] = remlen & 0xFF;
        remlen = remlen >> (8*i);
    }


    connectHeader* connectHdr = (connectHeader*) (mqtt->remLength + fieldCount);

    connectHdr->protocolNameLength = htons(4);
    char prtcl[4] = "MQTT";
    for(i =0; i < 4; i++)
       connectHdr->protocolName[i] = prtcl[i];

    connectHdr->protocolLevel = protocolLevel;
    connectHdr->keepAliveSeconds = htons(keepalive);
    connectHdr->connectFlag = connectFlags;

    char* connectData = (char*) connectHdr->data;
    getutf8Encoding(connectData, clientID, clientIDLength);

    uint16_t payloadLength = sizeof(mqttPack) + sizeof(connectHeader) + sizeof(connectData);


    return payloadLength;
}



