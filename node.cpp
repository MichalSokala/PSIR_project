#include <ZsutEthernet.h>
#include <ZsutEthernetUdp.h>
#include <ZsutFeatures.h>

#define MAC {0x01, 0xaa, 0xbb, 0xcc, 0xde, 0xe1} //nwm czy to jest potrzebne


#define PORT 8080
#define join_game // trzeba przypisac wartosc do makra

ZsutIPAddress address_ip=ZsutIPAddress(192,168,56,103); //adres ip serwera
char message[MAX_BUFFER];
char packetBuffer[MAX_BUFFER];
unsigned char sendBuffer[MAX_BUFFER];
unsigned int localPort;
int r;
uint16_t t;
bool game_started; // do wykorzystanie na zasadzie if game_started ==  True ... else ...


uint32_t last_serv_comm_time;// moze sie przydac jesli chcemy miec tez w wezle co ma sie stac jak
// serwer przestanie dzialac

ZsutEthernetUDP Udp;
byte mac[]=MAC;




void setup(){
    Serial.begin(9600);
    Serial.println(F("DEBUG: setup() START"));
    ZsutEthernet.begin(mac);
    Serial.println(ZsutEthernet.localIP());
    localPort = PORT;
    Udp.begin(localPort);

    Serial.println(F("DEBUG: send joining the game mess"));


}





void loop(){
    do{


        }while(1);
}
