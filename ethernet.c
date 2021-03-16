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

// Pins
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4

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
        putsUart0("Link is up\n");
    else
        putsUart0("Link is down\n");
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


//function which stores the string entered in the UART by the user
void getsUart0(USER_DATA* d)
{
  uint8_t c=0; //counter variable
  char ch;
  while (1)  //loop starts
  {

    ch=getcUart0();
    if ((ch==8 || ch==127) && c>0) c--;

    else if (ch==13)
        {
         d->buffer[c]=0;
         return;
        }
    else if (ch>=32)
     {
        d->buffer[c]=ch;
        //putcUart0(ch);
        c++;
        if (c==MAX_CHARS)
        {
            d->buffer[c]='\0';
            return;
        }
     }
     else continue;
  }
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
        else if(!((temp>=97 && temp<=122) || (temp>=65&&temp<=90)) && !(temp>=48 && temp<=57) )
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

    // Init controller
    initHw();

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
    displayConnectionInfo();

    // Flash LED
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);


    uint8_t mqtt_ip[4];
    uint8_t mqtt_addr[6];
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
            putsUart0("\n");

            parseFields(&d);

            bool valid=false;

            if(isCommand(&d,"connect",4))
            {
             mqtt_ip[0] = getFieldInteger(&d, 1);
             mqtt_ip[1] = getFieldInteger(&d, 2);
             mqtt_ip[2] = getFieldInteger(&d, 3);
             mqtt_ip[3] = getFieldInteger(&d, 4);

             putsUart0("Sending ARP Request to the MQTT Broker.\n\r");
             etherSendArpRequest(data, mqtt_ip);

             valid = true;
            }

            if(!valid)
                putsUart0("invalid command\n\r");

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

            // Handle ARP request
            if (etherIsArpRequest(data))
            {
                etherSendArpResponse(data);
            }

            //Handle ARP response
            if (etherIsArpResponse(data))
            {
                putsUart0("getting ARP response\n\r");
                //mqtt_addr

                arpPacket *arp = (arpPacket*)data->data;

                mqtt_addr[0] = arp->sourceAddress[0];
                mqtt_addr[1] = arp->sourceAddress[1];
                mqtt_addr[2] = arp->sourceAddress[2];
                mqtt_addr[3] = arp->sourceAddress[3];
                mqtt_addr[4] = arp->sourceAddress[4];
                mqtt_addr[5] = arp->sourceAddress[5];

                char text[50];
                sprintf(text, "mqtt address: %02x : %02x : %02x : %02x : %02x : %02x\n\r", mqtt_addr[0],mqtt_addr[1],mqtt_addr[2],mqtt_addr[3],mqtt_addr[4],mqtt_addr[5]);
                putsUart0(text);

                socket soc;
                uint8_t packet[MAX_PACKET_SIZE];

                uint8_t macAddr[6];
                etherGetMacAddress(macAddr);
                uint8_t ipAddr[4];
                etherGetIpAddress(ipAddr);

                uint8_t i = 0;
                for (i = 0; i < 6; i++)
                {
                    soc.sourceHw[i] = macAddr[i] ;
                    soc.destHw[i] = mqtt_addr[i];
                }


                for (i = 0; i < 4; i++)
                {
                    soc.sourceIp[i] = ipAddr[i];
                    soc.destIP[i] = mqtt_ip[i];
                }

                soc.destPort = 1883;
                soc.sourcePort = 12343;

                etherSendTCP(packet, &soc, 2);    //flag = 2 for SYN

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

                    // Process UDP datagram
                    if (etherIsUdp(data))
                    {
                        udpData = etherGetUdpData(data);
                        if (strcmp((char*)udpData, "on") == 0)
                            setPinValue(GREEN_LED, 1);
                        if (strcmp((char*)udpData, "off") == 0)
                            setPinValue(GREEN_LED, 0);
                        etherSendUdpResponse(data, (uint8_t*)"Received", 9);
                    }
                }
            }
        }
    }
}
