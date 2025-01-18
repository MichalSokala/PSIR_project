#include <ZsutEthernet.h>
#include <ZsutEthernetUdp.h>
#include <ZsutFeatures.h>
#include <stdio.h>
#include <sys/types.h>


#define MAC {0x01, 0xaa, 0xbb, 0xcc, 0xde, 0xe1} //nwm czy to jest potrzebne
#define REGISTER_MSG_TYPE 1
#define TOSS_RESULT_MSG_TYPE 2
#define PATTERN_MATCH_MSG_TYPE 3
#define CLIENT_ID 1
#define EMPTY_PACKAGE_TYPE 0
#define MAX_REGISTER_PACKET_LEN 2
//#define GAME_END_MSG_TYPE 4 //zalezy czy chcemy wysylac info ze gra sie konczy

#define PORT 8080
#define join_game // trzeba przypisac wartosc do makra

ZsutIPAddress address_ip=ZsutIPAddress(192,168,253,10); //adres ip serwera
char message[MAX_REGISTER_PACKET_LEN];
char packetBuffer[MAX_REGISTER_PACKET_LEN];
unsigned char sendBuffer[MAX_REGISTER_PACKET_LEN];
unsigned int localPort;
int r, packetsize;
uint16_t t;
bool game_started; // do wykorzystanie na zasadzie if game_started ==  True ... else ...


uint32_t last_serv_comm_time;// moze sie przydac jesli chcemy miec tez w wezle co ma sie stac jak
// serwer przestanie dzialac

ZsutEthernetUDP Udp;
byte mac[]=MAC;

typedef struct{
    uint8_t msg_type :3;
    uint8_t id :3;
    uint8_t retr_flag :1;
    uint8_t ack_flag :1;
    uint8_t p_type :2;
} Header;

void fillMessageBuffer(const Header *header, uint8_t *buffer) {
    if (header->msg_type > 7) {
        Serial.println(F("Error: msg_type out of range (0-7)\n"));
        return;
    }
    if (header->id > 7) {
        Serial.println(F("Error: id out of range (0-7)\n"));
        return;
    }
    if (header->retr_flag > 1) {
        Serial.println(F("Error: retr_flag out of range (0-1)\n"));
        return;
    }
    if (header->ack_flag > 1) {
        Serial.println(F("Error: ack_flag out of range (0-1)\n"));
        return;
    }
    if (header->p_type > 3) {
        Serial.println(F("Error: p_type out of range (0-3)\n"));
        return;
    }
    buffer[0] = (header->msg_type << 5) | (header->id << 2) | (header->retr_flag << 1) | header->ack_flag;
    buffer[1] = (header->p_type << 6);


}
void sendRegisterMsg(Header header, uint8_t *buffer){
    memset(&header, 0, sizeof(Header));
    header = { .msg_type = REGISTER_MSG_TYPE, .id = CLIENT_ID, .retr_flag = 0, .ack_flag = 0, .p_type = EMPTY_PACKAGE_TYPE };

   buffer[0] = (header.msg_type << 5) | (header.id << 2) | (header.retr_flag << 1) | header.ack_flag;
   buffer[1] = (header.p_type << 6);

   if (Udp.beginPacket(address_ip, PORT)) {
        int r = Udp.write(buffer, sizeof(buffer));
        if (r != sizeof(buffer)) {
            char message[100];
            snprintf(message, sizeof(message), "Error: Failed to write complete message, buffer_Size: %d, r:%d", sizeof(buffer), r);
            Serial.println(message);
        }
        if (!Udp.endPacket()) {
            Serial.println(F("Error: Failed to send UDP packet."));
        }
    } else {
        Serial.println(F("Error: Failed to begin UDP packet."));
    }
    last_serv_comm_time = ZsutMillis();
}

//void recvseqelement(uint8_t *sequence, uint16_t max_len){
//    packetsize = Udp.parsePacket();
//    if(packetsize){
//        Serial.print(F("DEBUG: received packet the size of: ");
//        Serial.print(packetsize);
//
//        if(packetsize > MAX_REGISTER_PACKET_LEN){
//            Serial.print(F("WARNING: UDP buffer size exceeded, received:"));
//            Serial.println(packetSize);
//        }
//        r = Udp.read(packetBuffer, MAX_REGISTER_PACKET_LEN);
//        if(MAX_REGISTER_PACKET_LEN >= packetsize){
//            uint16_t bit_index = 0;
//            Serial.println(F("Received data: "))
//              for (int i = 0; i < packetsize; i++){
//                  for (int j = 7; j >= 0; j--){
//                      if (bit_index < max_len){
//                              sequence[bit_index] = (packetBuffer[i] >> j) & 1;
//                                bit_index++;
//                          }
//                      else{
//                          break;
//                          }
//                    }
//                    if(bit_index >= max_len) break;
//              }
//                  Serial.println(F("received bit sequence: "));
//
//       }
//    }
//}


void setup(){
    Serial.begin(9600);
    Serial.println(F("DEBUG: setup() START"));
    ZsutEthernet.begin(mac);
    Serial.println(ZsutEthernet.localIP());
    localPort = PORT;
    Udp.begin(localPort);
    Header header;
    uint8_t buffer[2] = {0};



    Serial.println(F("DEBUG: send joining the game mess"));
    sendRegisterMsg(header, buffer);
    memset(&header, 0, sizeof(Header));

    Serial.println(F("DEBUG: send joining the game mess END"));

    Serial.println(F("DEBUG: setup() END"));
}





void loop(){
//    Serial.println(F("LOOP"));

}
