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
#define BUFFER_SIZE 20 // IN BYTES
#define PLAYER_GATHERING_TIME 15 // IN SECONDS
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

void  print_formatted_time(){
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    char time_str[23];
    strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S", local_time);
    printf("%s.%03d] ", time_str, tv.tv_usec/1000);
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

int send_toss_result_to_all(int sock, Player *player_list, int player_count,  uint8_t result){
    uint8_t *buffer, id;
    struct sockaddr_in receiver;
    int i, res;
    Header header = {
        .msg_type = TOSS_RESULT_MSG_TYPE,
        .retr_flag = 0,
        .ack_flag = 0,
        .p_type = 1
    };

    for(i = 0; i < player_count; i++){
        header.id = player_list[i].id;
        receiver = player_list[i].addr;

        pack_header(&header, buffer);
        buffer[1] = buffer[1] | (result & 0x01);

        res = sendto(sock, buffer, sizeof(buffer), 0,(struct sockaddr *) &receiver, sizeof(receiver));
        if (res < 0) {
            printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
            return -1;
        }
    }
}

int add_player(uint8_t id, Player *player_list, int *player_count, struct sockaddr_in c){
    if (*player_count >= MAX_PLAYERS) {
        printf("Player list is full.\n");
        return -1;
    }
    player_list[*player_count].id = id;
    player_list[*player_count].addr = c;
    (*player_count)++;
    return 0;
}

void acknowledge_response(){}
void save_winner(){}
int make_one_game(int sock, Player *player_list, int player_count) {
    uint8_t result;
    uint16_t payload;
    struct sockaddr_in c;
    int select_res, recv_res, c_len = sizeof(c);
    fd_set readfs;
    uint8_t buffer[BUFFER_SIZE];

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    double last_time_tossed = 0, current_time;

    Header header;
    bool all_acks_received = true;
    printf("Starting a new game...\n");

    while(1){
        FD_ZERO(&readfs);
        FD_SET(sock, &readfs);
        select_res = select(sock + 1, &readfs, NULL, NULL, &tv);
        if(select_res){
            printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
            return -1;
        }else if(select_res > 0){
            recv_res = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&c, &c_len);
            if (recv_res < 0) {
                printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
                return -1;
            }

            unpack_header(buffer, &header);
            if(header.p_type == 1){
                payload = (buffer[1] >> 5) & 0x01;
            }else if(header.p_type == 2){
                payload = (buffer[1] & 0x63) | (buffer[2] >> SEQUENCE_LENGTH - 2); // THIS NEED TO BE CHANGED
                                                                                   // SINCE WE ARE PASSING WINNING SEQUENCE
                                                                                   // AND NUMBER OF TOSSES
            }
            if(header.msg_type == TOSS_RESULT_MSG_TYPE && header.ack_flag){
                acknowledge_response(); // we process acks from nodes before we start another coin toss
            }else if(header.msg_type == PATTERN_MATCH_MSG_TYPE){
                save_winner();
            }else{
                printf("Wrong type of message: %d", header.msg_type);
            }
        }

        current_time = get_time_s();
	    if(current_time - last_time_tossed >= SYMBOL_GEN_INTERVAL && all_acks_received){
            result = toss_coin();
            printf("Outcome of toss: %s\n", result ? "H" : "T" );
            send_toss_result_to_all(sock, player_list, player_count, result);
            last_time_tossed = current_time;
            all_acks_received = false;
        }
    }
}

int gather_players(int sock, Player *player_list){
    uint8_t buffer[BUFFER_SIZE];
    struct sockaddr_in c;
    int select_res, recv_res, c_len = sizeof(c), res, player_count=0;
    fd_set readfs;
    Header header;

    struct timeval tv;
    
    double start_time = get_time_s();
    while(1){
        FD_ZERO(&readfs);
        FD_SET(sock, &readfs);
        tv.tv_sec = 0;
        tv.tv_usec = 50000;
        select_res = select(sock + 1, &readfs, NULL, NULL, &tv);
        if(select_res < 0){
            printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
            return -1;
        }else if(select_res > 0){
            recv_res = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&c, &c_len);
            if (recv_res < 0) {
                printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
                return -1;
            }
            unpack_header(buffer, &header);

            if(header.msg_type == REGISTER_MSG_TYPE){

                res = add_player(header.id, player_list, &player_count, c);

                if(res < 0){
                    printf("Failed to register player: %s:%d)", __FILE__, __LINE__);
                    return -1;
                }else{
                    printf("Successfully added player(ID=%d IP=%s PORT=%d) to list\n", header.id, inet_ntoa(c.sin_addr), ntohs(c.sin_port));
                    printf("Current player count: %d\n", player_count);
                }
            }else{
                printf("Wrong type of message: %d", header.msg_type);
            }
        }
        if(get_time_s() - start_time > PLAYER_GATHERING_TIME){
            return player_count;
        }
    }
}

int main() {
	struct addrinfo h, *r = NULL;
	memset(&h, 0, sizeof(struct addrinfo));

	int s, game_counter = 0;
	int player_count = 0;
	Player player_list[MAX_PLAYERS];

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
		freeaddrinfo(r);
		exit(-1);
	}
	if (bind(s, r->ai_addr, r->ai_addrlen) != 0) {
		printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
		freeaddrinfo(r);
        close(s);
		exit(-1);
	}

	srand(time(NULL));
    printf("Started gathering players...\n");
    player_count = gather_players(s, player_list);
    if(player_count <= 1){
        printf("ERROR: There is not enough players to start game %d < 2 (%s:%d)\n", player_count, __FILE__, __LINE__);
        freeaddrinfo(r);
        close(s);
		exit(-1);
    }

	for (int i = 0; i < NUMBER_OF_GAMES; i++) {
		make_one_game(s, player_list, player_count);
	}

	freeaddrinfo(r);
	close(s);
	return 0;
}
