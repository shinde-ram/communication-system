#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "function.h"

#define BUFLEN 1024
#define MSG_FIRST 1
#define MSG_ACK 2
#define MSG_NORMAL 3

int first = 1;

typedef struct firstMsg
{
	uint8_t isFirst;
	uint16_t portSender;
	uint8_t earCap;
	uint8_t mouthCap;
} firstMsg;

typedef struct normalMsg
{
	uint8_t isFirst;
	uint32_t len;
	char msg[BUFLEN];
} normalMsg;

void writeOutput(char *dir)
{
	int n;
	char buffer[BUFLEN];

	firstMsg *fmsg = malloc(sizeof(firstMsg));
	FILE *eb = openFile(dir, "eb.txt", "rb");
	if (first == 1)
	{
		fread(fmsg, sizeof(firstMsg), 1, eb);
	}
	else
	{
		fread(&n, sizeof(int), 1, eb);
		if (n > BUFLEN)
		{
			fprintf(stderr, "writeOutput: message length %d exceeds buffer, truncating\n", n);
			n = BUFLEN;
		}
		fread(buffer, sizeof(char), n, eb);
	}
	fclose(eb);

	FILE *outputUI = openFile(dir, "outputUI.txt", "wb");
	if (first == 1)
	{
		fwrite(&fmsg->portSender, sizeof(uint16_t), 1, outputUI);
		fwrite(&fmsg->earCap, sizeof(uint8_t), 1, outputUI);
		fwrite(&fmsg->mouthCap, sizeof(uint8_t), 1, outputUI);
	}
	else
	{
		fwrite(&n, sizeof(int), 1, outputUI);
		fwrite(buffer, sizeof(char), n, outputUI);
	}
	fclose(outputUI);
	free(fmsg);
}

void recvMsg(char *dir)
{
	struct sockaddr_in senderAddr, receiverAddr;
	char buffer[BUFLEN];

	memset(&receiverAddr, 0, sizeof(receiverAddr));
	memset(&senderAddr, 0, sizeof(senderAddr));

	FILE *be = openFile(dir, "be.txt", "rb");

	int RECVPORT;
	fread(&RECVPORT, sizeof(int), 1, be);

	fclose(be);

	int sockfd =
		setSocketToSendData(&receiverAddr, RECVPORT);

	if (bind(sockfd,
			 (const struct sockaddr *)&receiverAddr,
			 sizeof(receiverAddr)) < 0)
	{
		perror("bind syscall failed");
		close(sockfd);
		exit(EXIT_FAILURE);
	}

	printf("System 1 receiver waiting on port %d...\n",
		   RECVPORT);

	while (1)
	{
		memset(buffer, 0, sizeof(buffer));

		socklen_t len = sizeof(senderAddr);

		int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&senderAddr, &len);
		if (n < 0)
		{
			perror("recvfrom failed");
			continue;
		}

		if (n == 0)
			continue;

		uint8_t type = buffer[0];
		printf("Received packet type: %d\n", type);

		if (type == MSG_FIRST)
		{
			printf("ACK received from System 2\n");
			first = 0;

			FILE *eb = openFile(dir, "eb.txt", "wb");

			fwrite(&first, sizeof(int), 1, eb);
			fclose(eb);
			printf("Connection established.\n");
		}
		else if (type == MSG_NORMAL)
		{
			if (n < 1 + sizeof(int))
			{
				printf("Invalid normal message\n");
				continue;
			}
			int msgLen;
			memcpy(&msgLen, buffer + 1, sizeof(int));

			if (msgLen < 0 || msgLen > BUFLEN - 1 || msgLen > n - 1 - sizeof(int))
			{
				printf("Invalid message length\n");
				continue;
			}

			char message[BUFLEN];
			memcpy(message, buffer + 1 + sizeof(int), msgLen);
			message[msgLen] = '\0';

			printf("Normal message received: %s\n", message);

			FILE *eb = openFile(dir, "eb.txt", "wb");

			fwrite(&msgLen, sizeof(int), 1, eb);
			fwrite(message, sizeof(char), msgLen, eb);
			fclose(eb);
			writeOutput(dir);
		}
	}

	close(sockfd);
}

void sendFirstMsg(char *dir)
{
    struct sockaddr_in receiverAddr;
    FILE *bm = openFile(dir, "bm.txt", "rb");

    uint16_t SENDPORT;
    firstMsg fmsg;

    fread(&SENDPORT, sizeof(uint16_t), 1, bm);

    fread(&fmsg.isFirst, sizeof(uint8_t), 1, bm);
    fread(&fmsg.portSender, sizeof(uint16_t), 1, bm);
    fread(&fmsg.earCap, sizeof(uint8_t), 1, bm);
    fread(&fmsg.mouthCap, sizeof(uint8_t), 1, bm);

    fclose(bm);

    printf("SENDPORT = %u\n", SENDPORT);
    printf("isFirst = %u\n", fmsg.isFirst);
    printf("portSender = %u\n", fmsg.portSender);
    printf("earCap = %u\n", fmsg.earCap);
    printf("mouthCap = %u\n", fmsg.mouthCap);

    int sockfd = setSocketToSendData(&receiverAddr, SENDPORT);
    uint8_t buffer[5];

    buffer[0] = fmsg.isFirst;
    memcpy(buffer + 1, &fmsg.portSender, sizeof(uint16_t));

    buffer[3] = fmsg.earCap;
    buffer[4] = fmsg.mouthCap;
    int n = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr));

    printf("First message sent: %d bytes\n", n);
    FILE *mb = openFile(dir, "mb.txt", "wb");
    fwrite(&n, sizeof(int), 1, mb);
    fclose(mb);
    close(sockfd);
}
void sendMsg(char *dir)
{
	struct sockaddr_in receiverAddr;
	FILE *bm = openFile(dir, "bm.txt", "rb");
	uint16_t SENDPORT;
	int len;
	fread(&SENDPORT, sizeof(uint16_t), 1, bm);
	fread(&len, sizeof(int), 1, bm);

	if (len < 0){
		fprintf(stderr, "Invalid message length: %d\n", len);
		fclose(bm);
		return;
	}

	char message[BUFLEN];
	fread(message, sizeof(char), len, bm);
	int sockfd = setSocketToSendData(&receiverAddr, SENDPORT);

	uint8_t buffer[BUFLEN + 5];
	uint8_t type = MSG_NORMAL;

	memcpy(buffer, &type, sizeof(uint8_t));
	memcpy(buffer + sizeof(uint8_t), &len, sizeof(int));
	memcpy(buffer + sizeof(uint8_t) + sizeof(int), message, len);

	int packetLen = sizeof(uint8_t) + sizeof(int) + len;
	int n = sendto(sockfd, buffer, packetLen, 0, (const struct sockaddr *)&receiverAddr, sizeof(receiverAddr));
	printf("msg send\n");
	if (n < 0){
		perror("sendto failed");
	}
	else{
		printf("Normal message sent: %d bytes\n", n);
	}
	fclose(bm);

	FILE *mb = openFile(dir, "mb.txt", "wb");
	fwrite(&n, sizeof(int), 1, mb);
	fclose(mb);
	close(sockfd);
}

int main(int argc, char *argv[])
{
	if (argc < 4)
	{
		printf("Usage: ./executable <SENDPORT> <RECVPORT> <DIR_NAME>\n");
		exit(1);
	}
	uint16_t SENDPORT = atoi(argv[1]);
	int RECVPORT = atoi(argv[2]);
	char *dir = argv[3];

	mkdir(dir, 0777); // Create the directory if it doesn't exist

	// Create a file BE to store the receive port number
	createBE(dir, RECVPORT);
	printf("Starting the processes...\n");

	pid_t receiver = fork();

	if (receiver == 0)
	{
		recvMsg(dir); // waits for System 2's FIRST message
		exit(0);
	}

	pid_t sender = fork();

	if (sender == 0)
	{
		/* Take System 1's capabilities */
		takeInput(dir, first);

		/* Create FIRST message */
		createBM(dir, SENDPORT, first);

		while (first == 1)
		{
			sendFirstMsg(dir);
			FILE *eb = openFile(dir, "eb.txt", "rb");
			if (eb == NULL){
				sleep(5);
				printf("Retrying FIRST message...\n");
				continue;
			}

			fread(&first, sizeof(int), 1, eb);
			fclose(eb);
			if (first == 1){
				sleep(5);
				printf("System 2 FIRST message not received. Retrying...\n");
			}
		}

		printf("System 2 FIRST message received.\n");
		takeInput(dir, first);
		printf("this passs\n");
		createBM(dir, SENDPORT, first);
		printf("pass\n");
		sendMsg(dir);
		exit(0);
	}

	waitpid(sender, NULL, 0);
	waitpid(receiver, NULL, 0);
}