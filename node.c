#include <ZsutEthernet.h>
#include <ZsutEthernetUdp.h>
#include <ZsutFeatures.h>

#define MAC {0x01, 0xaa, 0xbb, 0xcc, 0xde, 0xe1}

#define PORT 1654
#define MAX_BUFFER 10

ZsutIPAddress address_ip=ZsutIPAddress(192,168,56,103); //client ip
char message[MAX_BUFFER];
char packetBuffer[MAX_BUFFER];
unsigned char sendBuffer[MAX_BUFFER];
int send_len=MAX_BUFFER;
int response_type;
uint16_t t;
uint32_t last_hello_time;
bool hello_answered;

ZsutEthernetUDP Udp;
byte mac[]=MAC;

void setup() {

    Serial.begin(9600);

    Serial.println(F("DEBUG: setup() START"));
    ZsutEthernet.begin(mac);
    Serial.println(ZsutEthernet.localIP());
    unsigned int localPort = PORT;
    Udp.begin(localPort);

    Serial.println(F("DEBUG: send hello START"));
    snprintf(message, MAX_BUFFER, "%1d%02d", HELLO_TYPE_1, SERVER_ID);

    Udp.beginPacket(address_ip, PORT);
    int r=Udp.write(message, send_len);
    Udp.endPacket();
    last_hello_time = ZsutMillis();
    Serial.println(F("DEBUG: send hello END"));

    Serial.println(F("DEBUG: setup() END"));
}

void loop() {
    int packetSize=Udp.parsePacket();
    if(packetSize){
        Serial.print("DEBUG: received packet the size of: ");
        Serial.println(packetSize);

        if(packetSize > MAX_BUFFER){
            Serial.print("WARNING: UDP buffer size exceeded, received:");
            Serial.println(packetSize);
        }

        int r=Udp.read(packetBuffer, MAX_BUFFER);
        snprintf(message, MAX_BUFFER, "%s", packetBuffer);

        Serial.print("DEBUG: got message: ");
        Serial.println((uint16_t)message[2]);

        response_type = packetBuffer[0] - '0';
        if(response_type==REQUEST_TEMP_MSG){
            Serial.println("DEBUG: temperature check start");
            t=ZsutAnalog5Read();
            Serial.print("read temperature: ");
            Serial.println(t);

            snprintf(message, MAX_BUFFER, "%1d%02d%04d", RESPONSE_TEMP_MSG, SERVER_ID, t);

            Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
            int r=Udp.write(message, send_len);

            Serial.print("DEBUG: sending current temperature start");
            Serial.print(r);
            Serial.println(" bytes");

            Udp.endPacket();

            Serial.println("DEBUG: sending current temperature end");

        }else if(response_type == IS_SERVER_ALIVE_MSG){
            snprintf(message, MAX_BUFFER, "%1d%02d", IS_SERVER_ALIVE_RESP, SERVER_ID);
            Serial.println(F("RECEIVED IS_SERVER_ALIVE_MSG MESSAGE"));

            Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
            int r=Udp.write(message, send_len);
            Udp.endPacket();
            Serial.println(F("DEBUG: respond to RECEIVED IS_SERVER_ALIVE_MSG MESSAGE"));
        }else if(response_type == HELLO_ACK_MSG){
            hello_answered = true;
            Serial.println(F("successfully connected with client"));
        }

    }
    if(ZsutMillis()-last_hello_time > HELLO_INTERVAL && hello_answered == false){
        Serial.println(F("DEBUG: resend hello START"));

        snprintf(message, MAX_BUFFER, "%1d%02d", HELLO_TYPE_1, SERVER_ID);

        Udp.beginPacket(address_ip, PORT);
        int r=Udp.write(message, send_len);
        Udp.endPacket();
        hello_answered = false;
        Serial.println(F("DEBUG: resend hello END"));

        last_hello_time = ZsutMillis();
    }
}