//IoT MQTT Broker Project
//Internet of Things Project 1

//Abhishek Dhital
//ID: 1001548204


// Ethernet Example
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL w/ ENC28J60
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// ENC28J60 Ethernet controller on SPI0
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS (SW controlled) on PA3
//   WOL on PB3
//   INT on PC6

// Pinning for IoT projects with wireless modules:
// N24L01+ RF transceiver
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS on PE0
//   INT on PB2
// Xbee module
//   DIN (UART1TX) on PC5
//   DOUT (UART1RX) on PC4


//-----------------------------------------------------------------------------
// Configuring Wireshark to examine packets
//-----------------------------------------------------------------------------

// sudo ethtool --offload eno2 tx off rx off
// in wireshark, preferences->protocol->ipv4->validate the checksum if possible
// in wireshark, preferences->protocol->udp->validate the checksum if possible

//-----------------------------------------------------------------------------
// Sending UDP test packets
//-----------------------------------------------------------------------------

// test this with a udp send utility like sendip
//   if sender IP (-is) is 192.168.1.1, this will attempt to
//   send the udp datagram (-d) to 192.168.1.199, port 1024 (-ud)
// sudo sendip -p ipv4 -is 192.168.1.1 -p udp -ud 1024 -d "on" 192.168.1.199
// sudo sendip -p ipv4 -is 192.168.1.1 -p udp -ud 1024 -d "off" 192.168.1.199

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "clock.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "wait.h"
#include "eth0.h"
#include "tm4c123gh6pm.h"
#include "mqtt.h"
#include "eeprom.h"

// Pins
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4


//Packet Types MQTT
//passed in as an argument to the getMQTTPacket function
#define CONNECT         1
#define CONNACK         2
#define PUBLISH         3
#define PUBACK          4
#define SUBSCRIBE       8
#define SUBACK          9
#define UNSUBSCRIBE     10
#define UNSUBACK        11
#define DISCONNECT      14


uint32_t sNum = 0;                  //global variable for sequence Number
uint32_t aNum = 0;                  //global variable for acknowledgement Number

//TCP State definitions
#define CLOSED          0
#define ARP_REQ         1
#define ARP_RES         2
#define SYN_SEND        3
#define WAIT_SYN_ACK    4
#define SEND_ACK        5
#define SENT_FIN1       6
#define SENT_FIN2       7
#define TIME_WAIT       8


//MQTT State definitions
#define STATE_CONNECT       9
#define STATE_CONNACK       10
#define STATE_SUB           11
#define STATE_SUBACK        12
#define STATE_PUBLISH       13
#define STATE_UNSUB         14
#define STATE_UNSUBACK      15
#define STATE_DISCONNECT    16
#define IDLE                17
#define PUBLISHED           18

uint16_t FSM_State    = IDLE;           //this variable is used to keep track of the current TCP FSM state and the MQTT connection state
bool EstablishedState = false;          //True when the TCP connection is established with the MQTT broker


#define PI                  23          //Packet Id for Subscribe and Unsubscribe commands

//EEPROM location where our data is stored for the MQTT broker IP and our own IP.
#define MQTTADDRESS     0x1
#define IPADDRESS       0x2

//-----------------------------------------------------------------------------
// Subroutines                
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    enablePort(PORTF);
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
    selectPinDigitalInput(PUSH_BUTTON);
}

void displayConnectionInfo()
{
    uint8_t i;
    char str[10];
    uint8_t mac[6];
    uint8_t ip[4];
    etherGetMacAddress(mac);
    putsUart0("HW: ");
    for (i = 0; i < 6; i++)
    {
        sprintf(str, "%02x", mac[i]);
        putsUart0(str);
        if (i < 6-1)
            putcUart0(':');
    }
    putcUart0('\n');
    etherGetIpAddress(ip);
    putsUart0("IP: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4-1)
            putcUart0('.');
    }
    if (etherIsDhcpEnabled())
        putsUart0(" (dhcp)");
    else
        putsUart0(" (static)");
    putcUart0('\n');
    etherGetIpSubnetMask(ip);
    putsUart0("SN: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4-1)
            putcUart0('.');
    }
    putcUart0('\n');
    etherGetIpGatewayAddress(ip);
    putsUart0("GW: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4-1)
            putcUart0('.');
    }
    putcUart0('\n');
    if (etherIsLinkUp())
        putsUart0("Link is up\n\r");
    else
        putsUart0("Link is down\n\r");
}



void displayStatus(uint8_t mqtt[])
{
    uint8_t ip[4];

    etherGetIpAddress(ip);
    putsUart0("\n\rIP: ");
    uint8_t i;
    char str[15];
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4-1)
            putcUart0('.');
    }

    putsUart0("\n\n\rMQTT: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", mqtt[i]);
        putsUart0(str);
        if (i < 4-1)
            putcUart0('.');
    }

    putsUart0("\n\r");

    putsUart0("TCP FInite State Machine State: ");

    if(EstablishedState)
        putsUart0("Established State\n\r");

    else
    {
        if (FSM_State == CLOSED)
            putsUart0("Closed\n\r");
        else if (FSM_State == SYN_SEND)
            putsUart0("Send Syn\n\r");
        else if (FSM_State == WAIT_SYN_ACK)
            putsUart0("Wait SYN ACK\n\r");
        else if (FSM_State == SEND_ACK)
            putsUart0("send ACK\n\r");
        else if (FSM_State == SENT_FIN1)
            putsUart0("FIN Wait-1\n\r");
        else if (FSM_State == SENT_FIN2)
            putsUart0("FIN Wait-2\n\r");
        else if (FSM_State == TIME_WAIT)
            putsUart0("Time Wait\n\r");
    }

    putsUart0("\n\rMQTT Connection State: ");

    if (FSM_State == IDLE)
        putsUart0("IDLE\n\r");

    else if (FSM_State <= STATE_CONNECT)
        putsUart0("No connection established\n\r");

    else if (FSM_State >= STATE_CONNACK)
        putsUart0("Connection establised\n\r");
}
//function that sets the bit in the NVIC APINT register so that the chip can reboot when the reboot command  is entered by the user
void reboot()
{
    putsUart0("Rebooting......\n\r");
    waitMicrosecond(100000);
    NVIC_APINT_R = 0x05FA0004;
}




//*************************Shell interface functions*************************************************
#define MAX_CHARS 80
#define MAX_FIELDS 5

typedef struct _USER_DATA
{
char buffer[MAX_CHARS+1];
uint8_t fieldCount;
uint8_t fieldPosition[MAX_FIELDS];
char fieldType[MAX_FIELDS];
} USER_DATA;

uint8_t counter = 0;        //counter for the getsUart0 function which keeps on incrementing unless a carriage return is entered.
bool tester = false;

//function which stores the string entered in the UART by the user
void getsUart0(USER_DATA* d)
{
  //uint8_t c=0; //counter variable
  char ch;
//  while (1)  //loop starts
//  {

    ch=getcUart0();
    if ((ch==8 || ch==127) && counter>0) counter--;

    else if (ch==13)
        {
         d->buffer[counter]=0;
         tester = true;
         return;
        }
    else if (ch>=32)
     {
        d->buffer[counter]=ch;
        //putcUart0(ch);
        counter++;
        if (counter==MAX_CHARS)
        {
            d->buffer[counter]='\0';
            tester = true;
            return;
        }
     }
     return;
//  }
}


//function which parses the given string and it is used in processing the commands
void parseFields(USER_DATA* d)
{
    uint8_t i=0;
    char prev=0;
    d->fieldCount=0;
    while(d->buffer[i]!='\0')
    {
        if((d->fieldCount)>=MAX_FIELDS)
        {
            break;
        }

        char temp=d->buffer[i];

        if(((temp>=97 && temp<=122) || (temp>=65&&temp<=90)) && prev!='a' )
        {
            prev='a';
            d->fieldType[(d->fieldCount)]='a';
            d->fieldPosition[(d->fieldCount)]=i;
            d->fieldCount+=1;
        }

        else if ((temp>=48 && temp<=57) && prev!='n')
           {
                prev='n';
                d->fieldType[d->fieldCount]='n';
                d->fieldPosition[d->fieldCount]=i;
                d->fieldCount+=1;
            }
        else if(!((temp>=97 && temp<=122) || (temp>=65&&temp<=90)) && !(temp>=48 && temp<=57))
           {
             prev=0;
             d->buffer[i]='\0';
           }
        i++;
   }
}

//function that gets the string from the input command, given the fieldNumber
char* getFieldString(USER_DATA* data, uint8_t fieldNumber)
{
  if(fieldNumber<=data->fieldCount)
      {
        return &(data->buffer[data->fieldPosition[fieldNumber]]);
      }
  else
      return -1;
}


//function to convert a number of data type-char to 32 bit integer
int32_t alphabetToInteger(char* numStr)
{
    int32_t num=0;
    while (*numStr != 0)
      {
        if(*numStr >= 48 && *numStr <= 57)
        {
              num = num*10 + ((*numStr) - 48);
              numStr++;
        }

      }
    return num;
}


//returns true if given two strings are equal
//false if not equal
bool stringCompare(const char* str1,const char* str2)
{
   bool equal = true;
   while(*str1 != 0 || *str2 != 0)
   {
       if((*str1 == 0 && *str2 != 0) || (*str1 != 0 && *str2 ==0))
           return false;

       if(!(*str1 == *str2 || (*str1 + 32) == *str2 || *str1 == (*str2+32) || (*str1 - 32) == *str2 || *str1 == (*str2 - 32)))
       {
           equal = false;
           break;
       }

       str1++;
       str2++;
   }
   return equal;
}


//returns the integer in the entered command, given the fieldNumber
int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber)
{
    if (fieldNumber<=data->fieldCount && data->fieldType[fieldNumber]=='n')
    {
        return alphabetToInteger(getFieldString(data, fieldNumber));
    }
    else
        return 0;
}



//function to check whether the entered command matches any of the kernel shell commands
//returns true if the entered command is valid
//false if invalid
bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments)
{
 if(stringCompare(strCommand,getFieldString(data,0)) && (data->fieldCount)>minArguments)
     return true;
 return false;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)
#define MAX_PACKET_SIZE 1522

int main(void)
{
    uint8_t* udpData;
    uint8_t buffer[MAX_PACKET_SIZE];
    etherHeader *data = (etherHeader*) buffer;
    ipHeader *ip = (ipHeader*)data->data;
    tcpHeader *tcp = (tcpHeader*)ip->data;


    // Init controller
    initHw();
    initEeprom();
    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);

    // Init ethernet interface (eth0)
    putsUart0("\nStarting eth0\n");
    etherSetMacAddress(2, 3, 4, 5, 6, 100);
    etherDisableDhcpMode();
    etherSetIpAddress(192, 168, 2, 100);
    etherSetIpSubnetMask(255, 255, 255, 0);
    etherSetIpGatewayAddress(192, 168, 1, 1);
    etherInit(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
    waitMicrosecond(100000);
    //displayConnectionInfo();

    // Flash LED
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);

    uint8_t mqtt_ip[4];
    uint8_t mqtt_addr[6];

    socket soc;         //socket variable that will be used to pass to the sendTCP function
    soc.destPort   = 1883;
    soc.sourcePort = 12343;
    uint16_t tcpPayloadSize = 0;


    //Read EEPROM to get the MQTT broker IP and IP for the client device
    if(readEeprom(MQTTADDRESS) == 0xFFFFFFFF)
        putsUart0("MQTT Broker IP not set. It needs to be set to establish a connection.\n\r");
    else
    {
        uint32_t temp = readEeprom(MQTTADDRESS);
        mqtt_ip[3] = temp & 0xFF;
        mqtt_ip[2] = (temp >> 8) & 0xFF;
        mqtt_ip[1] = (temp >> 16) & 0xFF;
        mqtt_ip[0] = (temp >> 24) & 0xFF;
    }

    if(readEeprom(IPADDRESS) == 0xFFFFFFFF)
        putsUart0("Client IP not set. It needs to be set to establish a connection.\r\n");
    else
    {
        uint32_t temp = readEeprom(IPADDRESS);
        etherSetIpAddress((temp >> 24) & 0xFF, (temp >> 16) & 0xFF, (temp >> 8) & 0xFF, temp & 0xFF);
    }



    // Main Loop
    // RTOS and interrupts would greatly improve this code,
    // but the goal here is simplicity
    while (true)
    {
        USER_DATA d;
        // Put terminal processing here
        if (kbhitUart0())
        {
            getsUart0(&d);
            //putsUart0("\n");
            if (tester)
            {
                counter = 0;
                tester = false;
                putsUart0("\n\r");
                parseFields(&d);

                bool valid=false;

                if(isCommand(&d, "reboot", 0))
                    reboot();

                else if(isCommand(&d, "setMQTT", 4) && d.fieldCount == 5)
                    {
                        uint32_t mqttIp = (getFieldInteger(&d, 1) << 24) + (getFieldInteger(&d, 2) << 16) + (getFieldInteger(&d, 3) << 8) + getFieldInteger(&d, 4);
                        writeEeprom(MQTTADDRESS, mqttIp);
                        putsUart0("MQTT address updated in EEPROM.\n\r");

                        valid = true;
                    }

                else if(isCommand(&d, "setIP", 4) && d.fieldCount == 5)
                    {
                        uint32_t temp = (getFieldInteger(&d, 1) << 24) + (getFieldInteger(&d, 2) << 16) + (getFieldInteger(&d, 3) << 8) + getFieldInteger(&d, 4);
                        writeEeprom(IPADDRESS, temp);
                        putsUart0("Ip address updated in the EEPROM\n\r");
                        valid = true;
                    }



                else if(isCommand(&d, "status", 0))
                {
                    displayStatus(mqtt_ip);
                    valid = true;
                }

                else if(isCommand(&d,"connect",0))
                {
                    if((FSM_State == IDLE || FSM_State == CLOSED) && !EstablishedState)
                    {
                        FSM_State = ARP_REQ;
                        aNum = 0;
                        sNum = 0;
                    }

                    else
                    {
                        putsUart0("Can't connect right now. Please try again\n\r");
                    }

                    valid = true;
                }

                else if(isCommand(&d, "disconnect", 0))
                {
                    FSM_State = STATE_DISCONNECT;
                    valid = true;
                }

                else if(isCommand(&d, "publish", 2))
                {
                    FSM_State = STATE_PUBLISH;
                    valid = true;
                }

                else if(isCommand(&d, "subscribe", 1))
                {
                    FSM_State = STATE_SUB;
                    valid = true;
                }

                else if(isCommand(&d, "unsubscribe", 1))
                {
                    FSM_State = STATE_UNSUB;
                    valid = true;
                }

                if(!valid)
                    putsUart0("invalid command\n\r");
            }
        }
        switch (FSM_State)
        {
        case(ARP_REQ):
            {
                putsUart0("Sending ARP Request to the MQTT Broker.\n\r");
                etherSendArpRequest(data, mqtt_ip);

                FSM_State = ARP_RES;
                break;
            }

        case (SYN_SEND):
            {
                etherGetMacAddress(soc.sourceHw);
                etherGetIpAddress(soc.sourceIp);

                uint8_t i = 0;
                for (i = 0; i < 4; i++)
                     soc.destIP[i] = mqtt_ip[i];

                for (i = 0; i < 6; i++)
                    soc.destHw[i] = mqtt_addr[i];

                etherSendTCP(data, &soc, 0x2, aNum, sNum, 4);           //size 4 for the options field attached at the end of TCP packet, for the first SYN message, to identify it as a MSS packet.
                FSM_State = WAIT_SYN_ACK;
                break;
            }

        case (SEND_ACK):
            {
                etherSendTCP((uint8_t*)data, &soc, 0x10, aNum, sNum, 0);
                FSM_State = STATE_CONNECT;
                EstablishedState = true;
                setPinValue(BLUE_LED, EstablishedState);
                break;
            }

        case (STATE_CONNECT):
            {
                getMQTTPacket(tcp->data, CONNECT, 0);
                tcpPayloadSize = getConnectPacket(tcp->data, 0x04, 2, 25, "abhi", 4);          //flags = 0; keepAlive = 100
                etherSendTCP((uint8_t*)data, &soc, 0x18, aNum, sNum, tcpPayloadSize);
                FSM_State = STATE_CONNACK;
                break;

            }

        case  (STATE_SUB):
            {
                getMQTTPacket(tcp->data, SUBSCRIBE, 2);
                tcpPayloadSize = getSubscribePacket(tcp->data, PI, getFieldString(&d, 1));
                etherSendTCP((uint8_t*)data, &soc, 0x18, aNum, sNum, tcpPayloadSize);
                FSM_State = STATE_SUBACK;
                break;
            }

        case (STATE_UNSUB):
            {
                getMQTTPacket(tcp->data, UNSUBSCRIBE, 2);
                tcpPayloadSize = getSubscribePacket(tcp->data, PI, getFieldString(&d, 1));
                etherSendTCP((uint8_t*)data, &soc, 0x18, aNum, sNum, tcpPayloadSize);
                FSM_State = STATE_UNSUBACK;
                break;
            }

        case (STATE_PUBLISH):
            {
                getMQTTPacket(tcp->data, PUBLISH, 0);       //only supports QoS = 0
                tcpPayloadSize = getPublishPacket(tcp->data, getFieldString(&d, 1), getFieldString(&d, 2));
                etherSendTCP((uint8_t*)data, &soc, 0x18, aNum, sNum, tcpPayloadSize);
                FSM_State = PUBLISHED;
                break;
            }

        case (STATE_DISCONNECT):
            {
                getMQTTPacket(tcp->data, DISCONNECT, 0);
                tcpPayloadSize = getDisconnectPacket(tcp->data);
                etherSendTCP((uint8_t*)data, &soc, 0x19, aNum, sNum, tcpPayloadSize);       //flags: FIN, ACK, PUSH
                FSM_State = SENT_FIN1;
                break;
            }

        }

        // Packet processing
        if (etherIsDataAvailable())
        {
            if (etherIsOverflow())
            {
                setPinValue(RED_LED, 1);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 0);
            }

            // Get packet
            etherGetPacket(data, MAX_PACKET_SIZE);
//
//            // Handle ARP request
//            if (etherIsArpRequest(data))
//            {
//                etherSendArpResponse(data);
//            }

            //Handle ARP response
            if (FSM_State == ARP_RES && etherIsArpResponse(data))
            {
                putsUart0("getting ARP response\n\r");
                //mqtt_addr

                arpPacket *arp = (arpPacket*)data->data;
                uint8_t i = 0;
                for(i = 0; i<6; i++)
                {
                    mqtt_addr[i] = arp->sourceAddress[i];
                }

                char text[50];
                sprintf(text, "mqtt address: %02x : %02x : %02x : %02x : %02x : %02x\n\r", mqtt_addr[0],mqtt_addr[1],mqtt_addr[2],mqtt_addr[3],mqtt_addr[4],mqtt_addr[5]);
                putsUart0(text);
                FSM_State = SYN_SEND;
                continue;
            }

            else if (FSM_State == ARP_RES && !etherIsArpResponse(data))
            {
                putsUart0("Couldn't get an ARP response from the MQTT broker.\n\r");
                FSM_State = ARP_REQ;
                continue;
            }

            // Handle IP datagram
            if (etherIsIp(data))
            {
                if (etherIsIpUnicast(data))
                {
                    // handle icmp ping request
                    if (etherIsPingRequest(data))
                    {
                      etherSendPingResponse(data);
                    }


                    //process TCP datagram
                    if(etherIsTcpResponse(data))
                    {
                        mqttPack* temp = (mqttPack*)tcp->data;

                        if ((temp->controlHeader >> 4) == PUBLISH)
                        {
                            FSM_State = IDLE;

                            uint16_t size = printPublishedPacket((uint8_t*)tcp->data);
                            aNum = 2 + size + ntohl(tcp->sequenceNumber);
                            etherSendTCP((uint8_t*)data, &soc, 0x10, aNum, sNum, 0);    //flag = 0x10 for ACK
                        }

                        switch(FSM_State)
                        {
                            case (WAIT_SYN_ACK):
                            {

                                //if received synack
                                if (ntohs(tcp->offsetFields) & 0x012)
                                {
                                    aNum = ntohl(tcp->sequenceNumber) + 1;
                                    sNum += 1;
                                    FSM_State = SEND_ACK;
                                }

                                else
                                {
                                    putsUart0("Error in receiving the SYN ACK.\n\r");
                                }
                                break;
                            }


                            case (SENT_FIN1):
                            {
                            if (ntohs(tcp->offsetFields) & 0x10) //if received ACK for the FIN
                                {
                                    FSM_State = SENT_FIN2;
                                    continue;
                               }
                                break;
                            }

                            case (SENT_FIN2):
                           {
                                if  (ntohs(tcp->offsetFields) & 0x01)   //if received FIN after ACK
                                {
                                    EstablishedState = false;
                                    sNum++;
                                    aNum = ntohl(tcp->sequenceNumber);
                                    aNum++;
                                    etherSendTCP((uint8_t*)data, &soc, 0x10, aNum, sNum, 0);    //send final ack before connection is closed.
                                    FSM_State = TIME_WAIT;
                                    waitMicrosecond(2000);          //timeout = 2MSL
                                    aNum = 0;
                                    sNum = 0;       //clearing the sequence and acknowledgement fields to use for next connection.
                                    FSM_State = CLOSED;
                                }
                                break;
                           }


                            case(STATE_CONNACK):
                            {
                                mqttPack* mqtt = (mqttPack*)tcp->data;
                                if (mqtt->controlHeader & 0x32)     //if it's a connack packet
                                {
                                    connackHeader* connack = (connackHeader*)(mqtt->remLength + 1);       //since it is a connack packet, we can assume that the remaining length field only occupies 1 byte.
                                    if (connack->returnCode == 0)
                                    {
                                        FSM_State = IDLE;
                                        aNum = getLength(data) + ntohl(tcp->sequenceNumber);
                                        sNum += tcpPayloadSize;

                                        etherSendTCP((uint8_t*)data, &soc, 0x10, aNum, sNum, 0);    //flag = 0x10 for ACK
                                        //putsUart0("Connected to the Broker!.\n\r");
                                    }

                                    else
                                    {
                                        putsUart0("Connack Error. (Connect request not accepted?).\n\r");
                                    }

                                }

                                break;
                            }

                            case (STATE_SUBACK):
                            {
                                mqttPack* mqtt = (mqttPack*)tcp->data;
                                if((mqtt->controlHeader >> 4) == SUBACK)        //if it's a SUBACK packet
                                {
                                    subAckHeader* subAck = (subAckHeader*) (mqtt->remLength + 1);         //since it is a suback packet, we can assume that the remaining length field only occupies 1 byte.
                                    if(htons(subAck->id) == PI && subAck->return_code == 0x00)
                                    {
                                        FSM_State = IDLE;
                                        aNum = getLength(data) + ntohl(tcp->sequenceNumber);
                                        sNum += tcpPayloadSize;

                                        etherSendTCP((uint8_t*)data, &soc, 0x10, aNum, sNum, 0);    //flag = 0x10 for ACK
                                        putsUart0("\n\rSubAck received. Going to IDLE State.\n\r");
                                    }

                                    else
                                    {
                                        putsUart0("SubAck received, but either packet Id or Return code is not valid.\n\r");
                                    }
                                }

    //                            else
    //                            {
    //                                putsUart0("Received packet was not a subAck. Trying again....\n\r");
    //                            }
                                break;
                            }

                            case (STATE_UNSUBACK):
                            {
                                mqttPack* mqtt = (mqttPack*)tcp->data;
                                if((mqtt->controlHeader >> 4) == UNSUBACK)
                                {
                                    unSubAckHeader* unSubAck = (unSubAckHeader*) (mqtt->remLength + 1);       //since it is a unsuback packet, the remaining length field is of 1 byte and the value is 0x2 by default
                                    if(htons(unSubAck->id) == PI)
                                    {
                                        FSM_State = IDLE;
                                        aNum = getLength(data) + ntohl(tcp->sequenceNumber);
                                        sNum += tcpPayloadSize;

                                        etherSendTCP((uint8_t*)data, &soc, 0x10, aNum, sNum, 0);
                                        putsUart0("\n\rUnSubAck received. Going to IDLE State.\n\r");
                                    }
                                }
                                break;
                            }

                            case (PUBLISHED):       //Only supports QoS = 0 for now
                            {
                                sNum += tcpPayloadSize;
                                break;
                            }


                        }


                    }
                }
            }
        }
    }
}
