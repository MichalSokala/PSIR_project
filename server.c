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

#define TOSSES_LIMIT  12 // IN BITS
#define SEQUENCE_LENGTH 10 // IN BITS
#define PORT "8080"
#define NUMBER_OF_GAMES 5
#define MAX_PLAYERS 8
#define SYMBOL_GEN_INTERVAL 2 // IN SECONDS
#define BUFFER_SIZE 20 // IN BYTES
#define PLAYER_GATHERING_TIME 15 // IN SECONDS
#define RETR_TIME 2 // IN SECONDS
#define TIMEOUT 5 // IN SECONDS

typedef struct{
    uint8_t msg_type :3;
    uint8_t id :3;
    uint8_t retr_flag :1;
    uint8_t ack_flag :1;
    uint8_t p_type :2;
    uint32_t payload :22;
} Header;

typedef struct{
    uint8_t id;
    struct sockaddr_in addr;
    bool result_ack;
    bool game_over_ack;
    uint16_t sequence :SEQUENCE_LENGTH;
} Player;

typedef enum {
    RESULT_ACK,
    GAME_OVER_ACK
} AckType;

typedef struct{
    uint16_t sequence :SEQUENCE_LENGTH;
    uint16_t toss_count :TOSSES_LIMIT;
} Result;


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
    if(header->p_type == 1){
        buffer[1] = (header->payload << 5);
    }else if(header->p_type == 2){
        buffer[1] = buffer[1] | (header->payload >> 16);
        buffer[2] = (header->payload >> 8) & 0xFF;
        buffer[3] = header->payload & 0xFF;
    }else if(header->p_type == 3){
        buffer[1] = buffer[1] | header->payload >> 4;
        buffer[2] = header->payload << 6;
    }
}

void unpack_header(uint8_t *buffer, Header *header){
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
        header->payload = buffer[2] >> 4 & 0xF;
    }
}

int send_toss_result_to_all(int sock, Player *player_list, int player_count,  uint8_t result){
    uint8_t *buffer, id;
    struct sockaddr_in receiver;
    int i, res;
    Header header = {
        .msg_type = TOSS_RESULT_MSG_TYPE,
        .retr_flag = 0,
        .ack_flag = 0,
        .p_type = 1,
        .payload = result & 0x01
    };

    for(i = 0; i < player_count; i++){
        header.id = player_list[i].id;
        receiver = player_list[i].addr;

        pack_header(&header, buffer);
        player_list[i].result_ack = false;

        res = sendto(sock, buffer, sizeof(buffer), 0,(struct sockaddr *) &receiver, sizeof(receiver));
        if (res < 0) {
            printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
            return -1;
        }
    }
    return 0;
}

int add_player(uint8_t id, Player *player_list, int *player_count, struct sockaddr_in c, uint16_t sequence){
    if (*player_count >= MAX_PLAYERS) {
        printf("Player list is full.\n");
        return -1;
    }
    player_list[*player_count].id = id;
    player_list[*player_count].sequence = sequence & 0x3FF;
    player_list[*player_count].addr = c;
    (*player_count)++;
    return 0;
}

bool are_all_acks_received(Player* player_list, uint8_t player_count, AckType ack_type) {
    for (uint8_t i = 0; i < player_count; i++) {
        if ((ack_type == RESULT_ACK && !player_list[i].result_ack) ||
            (ack_type == GAME_OVER_ACK && !player_list[i].game_over_ack)) {
            return false;
        }
    }
    return true;
}

int send_result_retransmissions(int sock, Player *player_list, uint8_t player_count, uint8_t result){
    uint8_t *buffer, id;
    struct sockaddr_in receiver;
    int i, res;
    Header header = {
        .msg_type = TOSS_RESULT_MSG_TYPE,
        .retr_flag = 1,
        .ack_flag = 0,
        .p_type = 1,
        .payload = result & 0x01
    };
    for(i = 0; i < player_count; i++){
        if(!player_list[i].result_ack){
            header.id = player_list[i].id;
            receiver = player_list[i].addr;

            pack_header(&header, buffer);

            res = sendto(sock, buffer, sizeof(buffer), 0,(struct sockaddr *) &receiver, sizeof(receiver));
            if (res < 0){
                printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
                return -1;
            }
        }
    }
}

void acknowledge_response(uint8_t player_id, Player *player_list, int player_count){
    uint8_t i;
    for(i = 1; i < player_count; i++){
        if(player_list[i].id == player_id){
            player_list[i].result_ack = true;
        }
    }
}

void save_winner(uint16_t sequence, uint16_t toss_count, Result *game_results, uint8_t game_number){
    Result result = {.sequence=sequence, .toss_count=toss_count};
    game_results[game_number] = result;
}

int send_game_over_to_all(int sock, Player *player_list, uint8_t player_count){
    uint8_t *buffer, id;
    struct sockaddr_in receiver;
    int i, res;
    Header header = {
        .msg_type = GAME_END_MSG_TYPE,
        .retr_flag = 0,
        .ack_flag = 0,
        .p_type = 0
    };
    for(i = 0; i < player_count; i++){
        header.id = player_list[i].id;
        receiver = player_list[i].addr;

        pack_header(&header, buffer);

        res = sendto(sock, buffer, sizeof(buffer), 0,(struct sockaddr *) &receiver, sizeof(receiver));
        if (res < 0){
            printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
            return -1;
        }
        player_list[i].game_over_ack = false;
    }
    return 0;
}

int send_game_over_retransmissions(int sock, Player *player_list, uint8_t player_count){
    uint8_t *buffer, id;
    struct sockaddr_in receiver;
    int i, res;
    Header header = {
        .msg_type = GAME_END_MSG_TYPE,
        .retr_flag = 1,
        .ack_flag = 0,
        .p_type = 0,
    };
    for(i = 0; i < player_count; i++){
        if(!player_list[i].game_over_ack){
            header.id = player_list[i].id;
            receiver = player_list[i].addr;

            pack_header(&header, buffer);

            res = sendto(sock, buffer, sizeof(buffer), 0,(struct sockaddr *) &receiver, sizeof(receiver));
            if (res < 0){
                printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
                return -1;
            }
        }
    }
}

void print_sequence_as_ht(uint16_t sequence){
    for(uint8_t i=0; i < SEQUENCE_LENGTH; i++){
        printf("%d", (sequence >> i) & 0x1) ? 'H': 'T';
    }
}

void print_probability(Player *player_list, uint8_t player_count, Result *game_results, uint8_t game_number){
    uint8_t counter;
    for(uint8_t i = 0; i < player_count; i++){
        counter = 0;
        for(uint8_t j = 0; j < game_number; j++){
            if(player_list[i].sequence == game_results[j].sequence){
                counter++;
            }
        }
        printf("Probability for");
        print_sequence_as_ht(player_list[i].sequence);
        if(!counter){
            printf(": %f%\n", 100*(float)counter/game_number);
        }else{
            printf(": 0.0%\n");
        }
    }
}

int make_one_game(int sock, Player *player_list, int player_count, Result *game_results, uint8_t game_number) {
    uint8_t result;
    uint16_t toss_count, winning_sequence;
    struct sockaddr_in c;
    int select_res, recv_res, c_len = sizeof(c);
    fd_set readfs;
    uint8_t buffer[BUFFER_SIZE];

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    double last_time_tossed = 0, current_time, last_result_ack_check, last_game_over_ack_check, game_over_first_sent=0;
    bool all_result_acks_received, found_winner = false, all_game_over_acks_received;
    Header header;
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
            if(header.p_type == 2){
                winning_sequence = header.payload >> 12;
                toss_count = header.payload & 0xFFF;
            }
            if(header.msg_type == TOSS_RESULT_MSG_TYPE && header.ack_flag){
                acknowledge_response(header.id, player_list, player_count);
            }else if(header.msg_type == PATTERN_MATCH_MSG_TYPE){
                if(!found_winner){
                    save_winner(winning_sequence, toss_count, game_results, game_number);
                    found_winner = true;
                }else{
                    printf("Winner has been already found\n");
                }
            }else{
                printf("Wrong type of message: %d\n", header.msg_type);
            }
        }

        current_time = get_time_s();
        all_result_acks_received = are_all_acks_received(player_list, player_count, RESULT_ACK);
        all_game_over_acks_received = are_all_acks_received(player_list, player_count, GAME_OVER_ACK);

        if(current_time - last_result_ack_check >= RETR_TIME && !all_result_acks_received){
            send_result_retransmissions(sock, player_list, player_count, result);
            last_result_ack_check = current_time;
        }
        if(current_time - last_game_over_ack_check >= RETR_TIME && !all_game_over_acks_received){
            send_game_over_retransmissions(sock, player_list, player_count);
            last_game_over_ack_check = current_time;
        }

        if((current_time - last_time_tossed >= TIMEOUT && !all_result_acks_received) ||
            (current_time - game_over_first_sent >= TIMEOUT && !all_game_over_acks_received)){
            printf("Lost connection with one of players. Ending game...\n");
            return -1;
        }

	    if(current_time - last_time_tossed >= SYMBOL_GEN_INTERVAL && all_result_acks_received){
            result = toss_coin();
            printf("Outcome of toss: %s\n", result ? "H" : "T" );
            send_toss_result_to_all(sock, player_list, player_count, result);
            last_time_tossed = current_time;
            last_result_ack_check = current_time;
        }

        if(found_winner){
	            send_game_over_to_all(sock, player_list, player_count);
	            found_winner = false;
	            game_over_first_sent = get_time_s();
	    }
	    if(all_game_over_acks_received){
	        return 0;
	    }


    };
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

                res = add_player(header.id, player_list, &player_count, c, header.payload);

                if(res < 0){
                    printf("Failed to register player: %s:%d)\n", __FILE__, __LINE__);
                    return -1;
                }else{
                    printf("Successfully added player(ID=%d IP=%s PORT=%d) to list\n", header.id, inet_ntoa(c.sin_addr), ntohs(c.sin_port));
                    printf("Current player count: %d\n", player_count);
                }
            }else{
                printf("Wrong type of message: %d\n", header.msg_type);
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
    Result results[NUMBER_OF_GAMES];
    uint8_t res;

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
	    res = make_one_game(s, player_list, player_count, results, i);
		if(res < 0){
		    break;
		}else{
		    printf("Game no. %d ended. Current probability for each used sequence:\n", i);
		    print_probability(player_list, player_count, results, i);
		}

	}

	freeaddrinfo(r);
	close(s);
	return 0;
}
