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

uint16_t getDisconnectPacket(uint8_t* tcpData)
{
    mqttPack* mqtt = (mqttPack*) tcpData;

    mqtt->remLength[0] = 0;

    return 2;   //size of disconnect packet is always = 2, which includes the controlHeader and the remaining length field (1 byte) which contains value 0x00.
}


uint16_t getConnectPacket(uint8_t* tcpData, uint8_t protocolLevel, uint8_t connectFlags, uint16_t keepalive, char* clientID, uint16_t clientIDLength)
{
    mqttPack* mqtt = (mqttPack*) tcpData;

    uint32_t remlen;
    uint8_t fieldCount = 1;
    remlen = getRemLength((sizeof(connectHeader) + sizeof(clientIDLength) + clientIDLength), &fieldCount);

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
    uint16_t sizeofData = sizeof(connectData);

    uint16_t payloadLength = 1 + fieldCount + sizeof(connectHeader) + sizeofData + sizeof(sizeofData);          //1-> size of Fixed Header (control header field), field COunt (no. of bytes for remaining length field);
    return payloadLength;
}

uint16_t getPublishPacket(uint8_t* tcpData, char* topic_name, char* data)
{
    mqttPack* mqtt = (mqttPack*) tcpData;

    uint32_t remlen;
    uint8_t fieldCount = 1;

    uint16_t topic_length = strlen(topic_name);
    remlen = getRemLength((sizeof(topic_length) + topic_length + strlen(data) + 2), &fieldCount);

    //initialize all indices to zero
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


    uint8_t* publish = mqtt->remLength + fieldCount;

    getutf8Encoding(publish, topic_name, topic_length);

    publish += 2 + topic_length;



    uint16_t sizeofPayload = strlen(data);


//
//    for (i = 0; i<sizeofPayload; i++)
//    {
//        *(publish + i) = *(data + i);
//    }

    getutf8Encoding(publish, data, sizeofPayload);

    uint16_t payloadLength = 1 + fieldCount + topic_length + sizeof(topic_length) + sizeofPayload + sizeof(sizeofPayload);
    return payloadLength;
}


uint16_t getSubscribePacket(uint8_t* tcpData, uint16_t packetId, char* topic_name)          //coded this routine such that it handles both the subscribe and unsubscribe packets.
{
    mqttPack* mqtt = (mqttPack*) tcpData;

    uint32_t remlen;
    uint8_t fieldCount = 1;

    uint16_t topic_length = strlen(topic_name);
    remlen = getRemLength(sizeof(packetId) + sizeof(topic_length) + topic_length + 1, &fieldCount);

    if (((mqtt->controlHeader >> 4) & 0xF) == 0xA)      //if unsubscribe packet;
        remlen--;


    //initialize all indices to zero
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

    uint8_t* subscribe = mqtt->remLength + fieldCount;

    *subscribe = htons(packetId) & 0xFF;
    subscribe++;
    *subscribe = (htons(packetId) >> 8) & 0xFF;
    subscribe++;

    getutf8Encoding(subscribe, topic_name, topic_length);
    subscribe += topic_length + 2;


    if (((mqtt->controlHeader >> 4) & 0xF) == 0xA)      //if unsubscribe packet;
    {

        uint16_t payloadLength = 1 + fieldCount + topic_length + sizeof(topic_length) + sizeof(packetId) ;  //1-> for the mqtt fixed header -> controlHeader field
        return payloadLength;
    }
        *subscribe = 0; //default QoS we are using = 0

   // getutf8Encoding(publish, data, sizeofPayload);

    uint16_t payloadLength = 2 + fieldCount + topic_length + sizeof(topic_length) + sizeof(packetId) ;  //2 -> 1 for the mqtt fixed header, 1 for the size of QoS field at the end
    return payloadLength;

}


