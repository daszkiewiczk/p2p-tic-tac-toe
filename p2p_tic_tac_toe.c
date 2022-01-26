#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>

key_t shmkey;
int shmid;
struct tic_tac_toe
{
	char square[9];
	int score[2];
	int my_turn;
	char my_mark;
	char opp_mark;
	char nick_opponent[16];
} *tic_tac_toe_data;

void sgnhandle(int signal) 
{
	printf("\nSIGINT");
	printf(" (detached: %s, removed: %s)\n",
		(shmdt(tic_tac_toe_data) == 0) ? "OK" : "shmdt",
		(shmctl(shmid, IPC_RMID, 0) == 0) ? "OK" : "shmctl");
	exit(0);
}
int exit_with_perror(char *msg) 
{
	perror(msg);
	exit(EXIT_FAILURE);
}
int setup_server() 
{
	int sock_fd;
	struct sockaddr_in server_addr;

	server_addr.sin_family      = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port        = htons(7312);

	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

	if (bind(sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
		exit_with_perror("bind");

	return sock_fd;
}
void initialize_board() 
{
	int i = 0;
	for (i = 0; i < 9; ++i)
	{
		tic_tac_toe_data->square[i] = 'a' + i;
	}
}
void initialize_score() 
{
	tic_tac_toe_data->score[0] = 0;
	tic_tac_toe_data->score[1] = 0;
}
void board() 
{
	int i = 0;
	for (i = 0; i < 3; ++i)
		printf("%c | %c | %c\n", tic_tac_toe_data->square[3 *i], tic_tac_toe_data->square[3 *i + 1], tic_tac_toe_data->square[3 *i + 2]);
}
void score() 
{
	printf("You %d : %d %s\n", tic_tac_toe_data->score[0], tic_tac_toe_data->score[1], tic_tac_toe_data->nick_opponent);
}
int mark_square(int x, int opp) 
{
	if (opp)
		tic_tac_toe_data->square[x] = tic_tac_toe_data->opp_mark;
	else
		tic_tac_toe_data->square[x] = tic_tac_toe_data->my_mark;
	return 0;
}
int checkwinner()
{
	if (tic_tac_toe_data->square[0] == tic_tac_toe_data->square[1] && tic_tac_toe_data->square[1] == tic_tac_toe_data->square[2])
		return 1;
	if (tic_tac_toe_data->square[3] == tic_tac_toe_data->square[4] && tic_tac_toe_data->square[4] == tic_tac_toe_data->square[5])
		return 1;
	if (tic_tac_toe_data->square[6] == tic_tac_toe_data->square[7] && tic_tac_toe_data->square[7] == tic_tac_toe_data->square[8])
		return 1;

	if (tic_tac_toe_data->square[0] == tic_tac_toe_data->square[3] && tic_tac_toe_data->square[3] == tic_tac_toe_data->square[6])
		return 1;
	if (tic_tac_toe_data->square[1] == tic_tac_toe_data->square[4] && tic_tac_toe_data->square[4] == tic_tac_toe_data->square[7])
		return 1;
	if (tic_tac_toe_data->square[2] == tic_tac_toe_data->square[5] && tic_tac_toe_data->square[5] == tic_tac_toe_data->square[8])
		return 1;

	if (tic_tac_toe_data->square[0] == tic_tac_toe_data->square[4] && tic_tac_toe_data->square[4] == tic_tac_toe_data->square[8])
		return 1;
	if (tic_tac_toe_data->square[2] == tic_tac_toe_data->square[4] && tic_tac_toe_data->square[4] == tic_tac_toe_data->square[6])
		return 1;

	return 0;
}
int main(int argc, char *argv[])
{
	int sock_fd;
	struct sockaddr_in client_addr, *peer_addr;
	struct addrinfo * peer_addr_info;
    
	int cpid;

	char option;

	char nick[16];

	signal(SIGINT, sgnhandle);
	if (argc == 1 || argc > 3)
	{
		printf("Usage: %s address [nick]\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (argc == 3)
	{
		strncpy(nick, argv[2], 16);
		nick[15] = '\0';
	}
	else
	{
		strcpy(nick, "NN");
	}

	sock_fd = setup_server();

	if (getaddrinfo(argv[1], "7312", NULL, &peer_addr_info) != 0)
		exit_with_perror("getaddrinfo");

	peer_addr = (struct sockaddr_in *)(peer_addr_info->ai_addr);
	client_addr.sin_family  = AF_INET;
	client_addr.sin_addr    = peer_addr->sin_addr;
	client_addr.sin_port    = htons(7312);

	char peer_addr_hr[INET_ADDRSTRLEN];
	if (inet_ntop(AF_INET, &(peer_addr)->sin_addr, peer_addr_hr, INET_ADDRSTRLEN) == NULL)
		exit_with_perror("inet_ntop");

	shmkey = ftok(".", 1);

	if ((shmid = shmget(shmkey, sizeof(struct tic_tac_toe), 0600 | IPC_CREAT | IPC_EXCL)) == -1)
		exit_with_perror("shmget");
	tic_tac_toe_data = (struct tic_tac_toe *)shmat(shmid, (void*) 0, 0);
	if (tic_tac_toe_data == (struct tic_tac_toe *) - 1)
		exit_with_perror("shmat");

	initialize_board();
	initialize_score();

	tic_tac_toe_data->my_mark = 'O';
	tic_tac_toe_data->opp_mark = 'X';
    strncpy(tic_tac_toe_data->nick_opponent, "NN", 3);
    
	printf("Starting a match with z %s. Write t to end. Write s to print current score\n", peer_addr_hr);

	option = 'p';
	if (sendto(sock_fd, &option, sizeof(option), 0, (struct sockaddr *) &client_addr, sizeof(client_addr)) == -1)
		exit_with_perror("sendto");

	if (sendto(sock_fd, &nick, sizeof(nick), 0, (struct sockaddr *) &client_addr, sizeof(client_addr)) == -1)
		exit_with_perror("sendto");
	cpid = fork();
	if (cpid == 0)
	{
		if (connect(sock_fd, (struct sockaddr *) &client_addr, sizeof(client_addr)) == -1)
			exit_with_perror("connect");
		while (1)
		{

			if (read(sock_fd, &option, sizeof(option)) == -1)
				exit_with_perror("read");

			if (option == 'p')
			{
				if (read(sock_fd, &(tic_tac_toe_data->nick_opponent), sizeof(tic_tac_toe_data->nick_opponent)) == -1)
					exit_with_perror("recvfrom");
				printf("\n[%s (%s) has joined]\n", tic_tac_toe_data->nick_opponent, peer_addr_hr);
				tic_tac_toe_data->my_turn = 1;
				tic_tac_toe_data->my_mark = 'X';
				tic_tac_toe_data->opp_mark = 'O';
				board();
				printf("[choose a field] ");
			}
			else if (option == 't')
			{
				printf("\n[%s (%s) has disconnected]\n", tic_tac_toe_data->nick_opponent, peer_addr_hr);
			}
			else
			{
				printf("\n[%s (%s) chose field %c]\n", tic_tac_toe_data->nick_opponent, peer_addr_hr, option);
				mark_square(option - 97, 1);
				tic_tac_toe_data->my_turn = 1;
				board();
				if (checkwinner())
				{
					tic_tac_toe_data->score[1] += 1;
					printf("[You've lost! Play again]\n");
					initialize_board();
					board();
				}
				printf("[choose a field] ");
			}
			fflush(stdout);
		}
		exit(EXIT_FAILURE);
	}
	else
	{

		if (connect(sock_fd, (struct sockaddr *) &client_addr, sizeof(client_addr)) == -1)
			exit_with_perror("connect");
		printf("[Proposal sent]\n");
		while (1)
		{
			scanf(" %c", &option);

			if (option == 's')
				score();
			else if (option == 't')
			{
				if (write(sock_fd, &option, sizeof(option)) == -1)
					exit_with_perror("write");
				kill(cpid, SIGINT);    
				close(sock_fd);        
				exit(EXIT_SUCCESS);
			}
			else
			{
				if (tic_tac_toe_data->my_turn == 1)
				{
					while (option - 97 < 0 || option - 97 > 8 || tic_tac_toe_data->square[option - 97] == 'X' || tic_tac_toe_data->square[option - 97] == 'O')
					{
						printf("[you cannot choose this field, choose another field] ");

						scanf(" %c", &option);
					}
					mark_square(option - 97, 0);
					if (checkwinner())
					{
						tic_tac_toe_data->score[0] += 1;
						printf("[You've won! Next game, wait for your turn] ");
						initialize_board();
					}
					if (write(sock_fd, &option, sizeof(option)) == -1)
						exit_with_perror("write");
					tic_tac_toe_data->my_turn = 0;
				}
				else if (tic_tac_toe_data->my_turn == 0)
				{
					printf("[it is other player's turn now, wait for your turn]\n");
				}
			}
		}
	}
	return 0;
}
