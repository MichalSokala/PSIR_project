#include <ZsutEthernet.h>
#include <ZsutEthernetUdp.h>
#include <ZsutFeatures.h>

#define MAC {0x01, 0xaa, 0xbb, 0xcc, 0xde, 0xe1}

#define PORT 1654
#define MAX_BUFFER 10

ZsutIPAddress address_ip=ZsutIPAddress(192,168,56,103); //client ip


ZsutEthernetUDP Udp;
byte mac[]=MAC;

void setup() {

}

void loop() {

    }