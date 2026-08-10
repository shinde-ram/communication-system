#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>

#define BUFLEN 1024

void createBM(char *dir, int port)
{
	char *path = malloc(strlen(dir) + strlen("bm.txt") + 1);
	sprintf(path, "%sbm.txt", dir);
	FILE *bm = fopen(path, "wb");

	path = malloc(strlen(dir) + strlen("inputUI.txt") + 1);
	sprintf(path, "%sinputUI.txt", dir);
	FILE *inputUI = fopen(path, "rb");
	int len;
	fread(&len, sizeof(int), 1, inputUI);
	char *messageStr = malloc(len + 1);
	fread(messageStr, sizeof(char), len, inputUI);
	fclose(inputUI);

	fwrite(&port, sizeof(int), 1, bm);
	fwrite(&len, sizeof(int), 1, bm);
	fwrite(messageStr, sizeof(char), len, bm);
	fclose(bm);
	free(messageStr);
}

void createBE(char *dir, int port)
{
	char *path = malloc(strlen(dir) + strlen("be.txt") + 1);
	sprintf(path, "%sbe.txt", dir);
	FILE *be = fopen(path, "wb");
	if (be == NULL){
		perror("Failed to create BE file");
		exit(EXIT_FAILURE);
	}
	fwrite(&port, sizeof(int), 1, be);
	fclose(be);
}

void writeOutput(char *dir){
	char *path;	
	int n;
	char buffer[BUFLEN];

	path = malloc(strlen(dir) + strlen("eb.txt") + 1);
	sprintf(path, "%seb.txt", dir);
	FILE *eb = fopen(path, "rb");
	fread(&n, sizeof(int), 1, eb);
	fread(buffer, sizeof(char), n, eb);
	fclose(eb);

	path = malloc(strlen(dir) + strlen("outputUI.txt") + 1);
	sprintf(path, "%soutputUI.txt", dir);
	FILE *outputUI = fopen(path, "wb");
	fwrite(&n, sizeof(int), 1, outputUI);
	fwrite(buffer, sizeof(char), n, outputUI);
	fclose(outputUI);	
}

void readMsg(char *dir)
{
	int sockfd;
	struct sockaddr_in senderAddr, receiverAddr;
	char buffer[BUFLEN];

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket system call failed");
		exit(EXIT_FAILURE);
	}

	memset(&receiverAddr, 0, sizeof(receiverAddr));
	memset(&senderAddr, 0, sizeof(senderAddr));

	
	char *path = malloc(strlen(dir) + strlen("be.txt") + 1);
	sprintf(path, "%sbe.txt", dir);
	FILE *be = fopen(path, "rb");
	int RECVPORT;
	fread(&RECVPORT, sizeof(int), 1, be);
	fclose(be);
	
	receiverAddr.sin_family = AF_INET;
	receiverAddr.sin_addr.s_addr = INADDR_ANY;
	receiverAddr.sin_port = htons(RECVPORT);

	if (bind(sockfd, (const struct sockaddr *)&receiverAddr, sizeof(receiverAddr)) < 0)
	{
		perror("bind syscall failed");
		exit(EXIT_FAILURE);
	}
	int len = sizeof(senderAddr);
	printf("Waiting for the data.... \n");
	int n = recvfrom(sockfd, (char *)buffer, BUFLEN, MSG_WAITALL, (struct sockaddr *)&senderAddr, &len);
	buffer[n] = '\0';

	path = malloc(strlen(dir) + strlen("eb.txt") + 1);
	sprintf(path, "%seb.txt", dir);
	FILE *eb = fopen(path, "wb");
	fwrite(&n, sizeof(int), 1, eb);
	fwrite(buffer, sizeof(char), n, eb);
	fclose(eb);

	writeOutput(dir);
	printf("data received: %s\n", buffer);
	close(sockfd);
}

void sendMsg(char *dir)
{
	struct sockaddr_in receiverAddr;
	int sockfd;

	char *path = malloc(strlen(dir) + strlen("bm.txt") + 1);
	sprintf(path, "%sbm.txt", dir);
	FILE *bm = fopen(path, "rb");
	int SENDPORT;
	int len;
	fread(&SENDPORT, sizeof(int), 1, bm);
	fread(&len, sizeof(int), 1, bm);
	char *messageStr = malloc(len + 1);
	fread(messageStr, sizeof(char), len, bm);
	fclose(bm);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}
	receiverAddr.sin_family = AF_INET;
	receiverAddr.sin_port = htons(SENDPORT);
	receiverAddr.sin_addr.s_addr = INADDR_ANY;

	sendto(sockfd, (const char *)messageStr, len, 0, (const struct sockaddr *)&receiverAddr, sizeof(receiverAddr));
	printf("Data send.....\n");

	path = malloc(strlen(dir) + strlen("mb.txt") + 1);
	sprintf(path, "%smb.txt", dir);
	FILE *mb = fopen(path, "wb");
	fwrite(&len, sizeof(int), 1, mb);
	fwrite(messageStr, sizeof(char), len, mb);
	fclose(mb);
	free(messageStr);
	close(sockfd);
}

// Create a file to store the input message
void takeInput(char *dir){
	char *path = malloc(strlen(dir) + strlen("inputUI.txt") + 1);
	sprintf(path, "%sinputUI.txt", dir);
	FILE *inputUI = fopen(path, "wb");
	if (inputUI == NULL)
	{
		perror("Failed to create inputUI file");
		exit(EXIT_FAILURE);
	}
	char *messagestr = malloc(BUFLEN);
	printf("Enter the message to send: ");
	fgets(messagestr, BUFLEN, stdin);
	// Remove the newline character from the message if present
	if (messagestr[strlen(messagestr) - 1] == '\n'){
		messagestr[strlen(messagestr) - 1] = '\0';
	}
	int len = strlen(messagestr);
	fwrite(&len, sizeof(int), 1, inputUI);
	fwrite(messagestr, sizeof(char), len, inputUI);
	fclose(inputUI);
}


int main(int argc, char *argv[])
{
	if(argc < 4 && argc < 5){
		printf("Usage: ./executable <SENDPORT> <RECVPORT> <DIR_NAME> <FLAG_TO_SEND>\n");
		exit(1);
	}
	int SENDPORT = atoi(argv[1]);
	int RECVPORT = atoi(argv[2]);
	char *dir = argv[3];

	printf("Here\n");	
	// Create a file BE to store the receive port number
	createBE(dir, RECVPORT);

	printf("Starting the processes...\n");

	pid_t pid = fork();
	if (pid == 0)
	{
		readMsg(dir);
	}

	if (argc < 5)
		wait(NULL);

	pid = fork();
	if (pid == 0)
	{
		// Create a file BM to store the port number and message
		takeInput(dir);
		createBM(dir, SENDPORT);
		sendMsg(dir);
	}
	wait(NULL);


}
