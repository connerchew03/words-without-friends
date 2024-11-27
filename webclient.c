// Conner Chew
// Web Client
// webclient.c

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
int main()
{
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	getaddrinfo("localhost", "8000", &hints, &res);
	
	int soc = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	char sendMsg[100] = {0}, recvMsg[100000] = {0};
	printf("Please enter a command: ");
	fgets(sendMsg, 99, stdin);
	connect(soc, res->ai_addr, res->ai_addrlen);
	
	send(soc, sendMsg, sizeof(sendMsg), 0);
	recv(soc, recvMsg, sizeof(recvMsg), 0);
	printf("%s\n", recvMsg);
	
	close(soc);
	freeaddrinfo(res);
}
