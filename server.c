#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <stdbool.h>



#define SEQUENCE_LENGTH 10
#define PORT "8080"
#define NUMBER_OF_GAMES 5


typedef struct{
    uint8_t msg_type :3;
    uint8_t id :3;
    uint8_t retr_flag :1;
    uint8_t ack_flag :1;
    uint8_t p_type :2;
} Header;

char toss_coin() {
    return rand() % 2 ? 1 : 0;
}

void make_one_game(int sock) {
	printf("Starting a new game...\n");

	uint8_t result = toss_coin();

	printf("Outcome of toss: %s\n", result ? "H" : "T" );
}

int main() {
	struct addrinfo h, *r = NULL;
	memset(&h, 0, sizeof(struct addrinfo));
	int s, game_counter = 0;
	int client_count = 0;
	bool is_game_running = false;

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
//	recvfrom(); //
//	sendto(); //

	// Initialize random number generator
	srand(time(NULL));

	printf("Generating random sequences of H and T:\n");
	for (int i = 0; i < NUMBER_OF_GAMES; i++) {
		make_one_game(s);
	}
	freeaddrinfo(r);

	close(s);
	return 0;
}
