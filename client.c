#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

int main(int argc, char *argv[]) {
	char address[256];
  char buffer[512];
	char line[128];
	char portStr[16];
  char fileSize[16];
  char fileName[21];
  char chunkSize[16];
	char *token;
	FILE *file;
	int sock;
	int len;
	int port;
  int sz;
	struct addrinfo hints;
	struct addrinfo *info;
	struct sockaddr_in dest;

	if(argc != 3) {
		printf("Format for client connection is ./client [[IP ADDRESS]]:[[PORT NUMBER]] [[FILE NAME TO SAVE]].\n");
		exit(0);
	}

	token = strtok(argv[1], ":");
	if(!token) {
		printf("Problem tokenizing the argument.\n");
		exit(0);
	}

	strcpy(address, token);

	token = strtok(NULL, ":");
	if(!token) {
		printf("Argument must be delimited by a colon (:).\n");
		exit(0);
	}

	port = atoi(token);
	if(!port) {
		printf("Port must be a number.\n");
		exit(0);
	}

	memset(&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	dest.sin_port = htons(port);

	if(strcmp(address, "127.0.0.1") != 0) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		sprintf(portStr, "%d", port);
		if(getaddrinfo(address, portStr, &hints, &info)) {
			printf("Problem resolving address.\n");
			exit(0);
		}

		struct sockaddr_in *temp = (struct sockaddr_in*)info->ai_addr;
		inet_pton(AF_INET, inet_ntoa(temp->sin_addr), &(dest.sin_addr));
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);

	file = fopen(argv[2], "r");
	if(!file) {
		printf("Error opening %s.\n", argv[2]);
		exit(0);
	}

	connect(sock, (struct sockaddr *)&dest, sizeof(struct sockaddr_in));
  
  fseek(file, 0, SEEK_END);
  sz = ftell(file);
  sprintf(fileSize, "%d", sz);
  fseek(file, 0, SEEK_SET);
  strcpy(chunkSize, "256");
  strcpy(fileName, argv[2]);
  
  send(sock, fileSize, strlen(fileSize), 0);
  recv(sock, buffer, 512, 0);
  send(sock, chunkSize, strlen(chunkSize), 0);
  recv(sock, buffer, 512, 0);
  send(sock, fileName, strlen(fileName), 0);
  recv(sock, buffer, 512, 0);
  
	while(fgets(line, sizeof(line), file)) {
		send(sock, line, strlen(line) + 1, 0);
    recv(sock, buffer, 512, 0);
	}

  printf("Finished sending.\n");
	close(sock);
	return 0;
}
