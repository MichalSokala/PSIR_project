#include <ZsutEthernet.h>
#include <ZsutEthernetUdp.h>
#include <ZsutFeatures.h>
#include <stdio.h>
#include <sys/types.h>


#define MAC {0x01, 0xaa, 0xbb, 0xcc, 0xde, 0xe1}
#define REGISTER_MSG_TYPE 1
#define TOSS_RESULT_MSG_TYPE 2
#define PATTERN_MATCH_MSG_TYPE 3
#define GAME_END_MSG_TYPE 4

#define EMPTY_PAYLOAD_TYPE 0
#define SINGLE_TOSS_PAYLOAD_TYPE 1
#define RESULT_PAYLOAD_TYPE 2
#define SEQUENCE_PAYLOAD_TYPE 3

#define CLIENT_ID 1
#define MAX_BUFFER_LEN 10
#define SEQUENCE_LENGTH 6
#define RETR_TIME 2000 // IN milliseconds

#define SEQUENCE 0b111111
#define PORT 8080
//#define join_game // trzeba przypisac wartosc do makra

ZsutIPAddress address_ip = ZsutIPAddress(192, 168, 253, 10); //adres ip serwera

uint8_t packetBuffer[MAX_BUFFER_LEN];
unsigned int localPort;
int r, packetSize;
uint16_t t;
bool winnerInfoAnswered=true, registerAnswered=true;
uint16_t sequence, tossCount;

uint32_t lastServRegisterMessTime, lastServWinnerInfMessTime;// moze sie przydac jesli chcemy miec tez w wezle co ma sie stac jak
// serwer przestanie dzialac

ZsutEthernetUDP Udp;
byte mac[] = MAC;

typedef struct {
    uint8_t msg_type: 3;
    uint8_t id: 3;
    uint8_t retr_flag: 1;
    uint8_t ack_flag: 1;
    uint8_t p_type: 2;
    uint32_t payload: 18;
} Header;

Header packetHeader;


void packMessageBuffer(Header *header, uint8_t *buffer) {
    buffer[0] = (header->msg_type << 5) | (header->id << 2) | (header->retr_flag << 1) | (header->ack_flag);
    buffer[1] = (header->p_type << 6);

    if(header->p_type == 1){
        buffer[1] = (header->payload << 5);
    }else if(header->p_type == 2){
        buffer[1] = buffer[1] | (header->payload >> 16);
        buffer[2] = (header->payload >> 8) & 0xFF;
        buffer[3] = header->payload & 0xFF;
    }else if(header->p_type == 3){
        buffer[1] = buffer[1] | header->payload;
    }
}

void unpackMessageBuffer(Header *header, uint8_t *buffer) {
    header->msg_type = (buffer[0] >> 5) & 0x07;
    header->id = (buffer[0] >> 2) & 0x07;
    header->retr_flag = (buffer[0] >> 1) & 0x01;
    header->ack_flag = buffer[0] & 0x01;
    header->p_type = (buffer[1] >> 6) & 0x03;

    if(header->p_type == 1){
        header->payload = (buffer[1] >> 5) & 0x01;
    }else if(header->p_type == 2){
        header->payload = buffer[1] & 0x3F;
        header->payload = header->payload | buffer[2];
        header->payload = header->payload | buffer[3];
    }else if(header->p_type == 3){
        header->payload = buffer[1] & 0x3F;
    }
    Serial.print("HEADER p_Type");
    Serial.println(header->p_type);
    Serial.print("HEADER payload");
    Serial.println(header->payload);
}

void sendMessage(Header header, uint8_t *buffer) {
    packMessageBuffer(&header, buffer);
    Udp.beginPacket(address_ip, PORT);
    Udp.write(buffer, sizeof(buffer));
    Udp.endPacket();
}


void sendRegisterMsg(uint8_t *buffer) {
    Header header;
    header = {
            .msg_type = REGISTER_MSG_TYPE,
            .id = CLIENT_ID,
            .retr_flag = 0,
            .ack_flag = 0,
            .p_type = SEQUENCE_PAYLOAD_TYPE,
            .payload = SEQUENCE
    };
    sendMessage(header, buffer);
    lastServRegisterMessTime = ZsutMillis();
}

void sendWinnerInfo(uint8_t *buffer) {
    Header header;
    header = {
            .msg_type = PATTERN_MATCH_MSG_TYPE,
            .id = CLIENT_ID,
            .retr_flag = 0,
            .ack_flag = 0,
            .p_type = RESULT_PAYLOAD_TYPE,
            .payload = ((sequence & 0x3F) << 12) | (tossCount & 0xFFF)
    };
    sendMessage(header, buffer);
    lastServWinnerInfMessTime = ZsutMillis();
}

void setup() {
    Serial.begin(9600);
    Serial.println(F("DEBUG: setup() START"));
    ZsutEthernet.begin(mac);
    Serial.println(ZsutEthernet.localIP());
    localPort = PORT;
    Udp.begin(localPort);
    uint8_t buffer[3];

    Serial.println(F("DEBUG: send joining the game mess START"));
    sendRegisterMsg(buffer);
    registerAnswered = false;
    Serial.println(F("DEBUG: send joining the game mess END"));
    Serial.println(F("DEBUG: setup() END"));
    tossCount = 0;
}

void loop() {
    packetSize = Udp.parsePacket();
    if (packetSize) {
        Serial.println(F("DEBUG: Received the packet size of: "));
        Serial.println(packetSize);

        if (packetSize > MAX_BUFFER_LEN) {
            Serial.println(F("WARNING: UDP buffer exceeded, got: "));
            Serial.println(packetSize);
        }
        uint8_t packetBuffer[MAX_BUFFER_LEN];
        r = Udp.read(packetBuffer, sizeof(packetBuffer));
        char message[100];
        snprintf(message, sizeof(message), "buffer0: %d buffer1: %d ", packetBuffer[1], packetBuffer[2]);
        Serial.println(message);
        Header packetHeader;
        unpackMessageBuffer(&packetHeader, packetBuffer);

        if(packetHeader.msg_type == TOSS_RESULT_MSG_TYPE) {
            Serial.println(F("DEBUG: recv single toss result start: "));
            tossCount++;
            packetHeader.ack_flag = 1;
            Serial.print("Toss result: ");
            Serial.println(packetHeader.payload);
            sendMessage(packetHeader, packetBuffer);
            sequence = (sequence << 1) | (packetHeader.payload & 0x1);
            Serial.print("Sequence to match: ");
            Serial.println(sequence);
            if (tossCount >= SEQUENCE_LENGTH && (sequence & 0x3F) == SEQUENCE) {
                Serial.println(F("DEBUG: FOUND matching pattern"));
                Serial.println(sequence);
                sendWinnerInfo(packetBuffer);
                winnerInfoAnswered = false;
            }
            Serial.println(F("DEBUG: recv single toss result end"));
        }else if(packetHeader.msg_type == GAME_END_MSG_TYPE) {
            Serial.println(F("This round has ended. Next one will start shortly."));
            sequence = 0;
            tossCount = 0;
            packetHeader.ack_flag = 1;
            Serial.println(F("Started sending GAME_END ack"));
            sendMessage(packetHeader, packetBuffer);
            Serial.println(F("Ended sending GAME_END ack"));
        }

//        }else if(packetHeader.msg_type == REGISTER_MSG_TYPE && packetHeader.ack_flag == 1){
//            registerAnswered = true;
//        }else if(packetHeader.msg_type == PATTERN_MATCH_MSG_TYPE && packetHeader.ack_flag == 1){
//            winnerInfoAnswered = true;
//        }
    }

//    if(ZsutMillis() - lastServRegisterMessTime > RETR_TIME && !registerAnswered){
//        packetHeader = {
//            .msg_type = REGISTER_MSG_TYPE,
//            .id = CLIENT_ID,
//            .retr_flag = 1,
//            .ack_flag = 0,
//            .p_type = SEQUENCE_PAYLOAD_TYPE,
//            .payload = SEQUENCE
//        };
//        sendMessage(packetHeader, packetBuffer);
//        registerAnswered = false;
//        lastServRegisterMessTime = ZsutMillis();
//    }
//
//    if(ZsutMillis() - lastServWinnerInfMessTime > RETR_TIME && !winnerInfoAnswered){
//        packetHeader = {
//            .msg_type = PATTERN_MATCH_MSG_TYPE,
//            .id = CLIENT_ID,
//            .retr_flag = 1,
//            .ack_flag = 0,
//            .p_type = RESULT_PAYLOAD_TYPE,
//            .payload = ((sequence & 0x3F) << 12) | (tossCount & 0xFFF)
//        };
//        sendMessage(packetHeader, packetBuffer);
//        winnerInfoAnswered = false;
//        lastServWinnerInfMessTime = ZsutMillis();
//    }
}