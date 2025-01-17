#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <stdbool.h>


#define REGISTER_MSG_TYPE 1
#define TOSS_RESULT_MSG_TYPE 2
#define PATTERN_MATCH_MSG_TYPE 3
#define GAME_END_MSG_TYPE 4

#define SEQUENCE_LENGTH 10
#define PORT "8080"
#define NUMBER_OF_GAMES 5
#define MAX_PLAYERS 8
#define SYMBOL_GEN_INTERVAL 2 // IN SECONDS

typedef struct{
    uint8_t msg_type :3;
    uint8_t id :3;
    uint8_t retr_flag :1;
    uint8_t ack_flag :1;
    uint8_t p_type :2;
} Header;

typedef struct{
    uint8_t id;
    struct sockaddr_in addr;
} Player;

char toss_coin() {
    return rand() % 2 ? 1 : 0;
}

double get_time_s(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec/1000000;
}

void pack_header(Header *header, uint8_t *buffer){
    buffer[0] = (header->msg_type << 5) | (header->id << 2) | (header->retr_flag << 1) | header->ack_flag;
    buffer[1] = (header->p_type << 6);
}

void unpack_header(uint8_t *buffer, Header *header){
    header->msg_type = (buffer[0] >> 5) & 0x07;
    header->id = (buffer[0] >> 2) & 0x07;
    header->retr_flag = (buffer[0] >> 1) & 0x01;
    header->ack_flag = buffer[0] & 0x01;
    header->p_type = (buffer[1] >> 6) & 0x03;
}

void add_player(Header header){
}
void acknowledge_response(){}
void save_winner(){}
void make_one_game(int sock) {
    uint8_t result;
    uint16_t payload;
    int select_res, recv_res, c_len = sizeof(c);
    struct sockaddr_in c;
    fd_set readfs;

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    double last_time_tossed = 0, current_time;

    Header header;
    printf("Starting a new game...\n");

    do{
        FD_ZERO(&readfs);
        FD_SET(sock, &readfs);
        select_res = select(sock + 1, &readfs, NULL, NULL, &tv);
        if(select_res){
            printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
            break;
        }

        recv_res = recvfrom(s, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&c, &c_len);
        if (recv_res < 0) {
            printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
            exit(-1);
        }

        unpack_header(buffer, header);
        if(header.p_type == 1){
            payload = (buffer[1] & 0x63) | buffer[2];
        }else if(header.p_type == 2){
            payload = (buffer[1] & 0x63) | (buffer[2] >> SEQUENCE_LENGTH - 2);
        }

        if(header.msg_type == REGISTER_MSG_TYPE){
            add_client();
        }else if(header.msg_type == TOSS_RESULT_MSG_TYPE && header.ack){
            acknowledge_response(); // we process acks from nodes before we start another coin toss
        }else if(header.msg_type == PATTERN_MATCH_MSG_TYPE){
            save_winner
        }else{
            printf("Wrong type of message: %d", header.msg_type);
        }

        current_time = get_time_s();
	    if(current_time - last_time_tossed >= SYMBOL_GEN_INTERVAL){
            result = toss_coin();
            printf("Outcome of toss: %s\n", result ? "H" : "T" );
            last_time_tossed = current_time;
        }
    }while(1);
}

int main() {
	struct addrinfo h, *r = NULL;
	memset(&h, 0, sizeof(struct addrinfo));

	int s, game_counter = 0;
	int client_count = 0;
	Player server_list[MAX_PLAYERS];

	h.ai_family = PF_INET;
	h.ai_socktype = SOCK_DGRAM;
	h.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, PORT, &h, &r) != 0) {
		printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
		exit(-1);
	}
	s = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
	if (s == -1) {
		printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
		exit(-1);
	}
	if (bind(s, r->ai_addr, r->ai_addrlen) != 0) {
		printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
        close(s);
		exit(-1);
	}

	srand(time(NULL));

	for (int i = 0; i < NUMBER_OF_GAMES; i++) {
		make_one_game(s);
	}

	freeaddrinfo(r);
	close(s);
	return 0;
}
