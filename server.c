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



void generate_random_sequence(char *sequence, int length) {
	for (int i = 0; i < length; i++) {
		sequence[i] = (rand() % 2 == 0) ? 1 : 0;
	}
	sequence[length] = '\0';

}

void make_one_game() {
	printf("Starting a new game...\n");

	char sequence[SEQUENCE_LENGTH + 1];   // Array to store the sequence (+1 for null terminator)
	generate_random_sequence(sequence, SEQUENCE_LENGTH);


	printf("Generated sequence: %s\n", sequence);
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
		make_one_game();
	}
	freeaddrinfo(r);

	close(s);
	return 0;
}
