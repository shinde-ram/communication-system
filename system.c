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
    uint8_t buffer[BUFLEN + 5];

    memset(&receiverAddr, 0, sizeof(receiverAddr));
    memset(&senderAddr, 0, sizeof(senderAddr));

    FILE *be = openFile(dir, "be.txt", "rb");

    int RECVPORT;
    fread(&RECVPORT, sizeof(int), 1, be);
    fclose(be);

    int sockfd = setSocketToSendData(&receiverAddr, RECVPORT);

    if (bind(sockfd, (struct sockaddr *)&receiverAddr,
             sizeof(receiverAddr)) < 0)
    {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("System 2 receiver started on port %d\n", RECVPORT);

    while (1)
    {
        socklen_t len = sizeof(senderAddr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&senderAddr, &len);

        if (n <= 0)
            continue;
        uint8_t type = buffer[0];

        if (type == MSG_FIRST)
        {
            if (n < 5){
                printf("Invalid first message\n");
                continue;
            }

            uint16_t portSender;
            uint8_t earCap;
            uint8_t mouthCap;

            memcpy(&portSender, buffer + 1, sizeof(uint16_t));
            memcpy(&earCap, buffer + 3, sizeof(uint8_t));
            memcpy(&mouthCap, buffer + 4, sizeof(uint8_t));

            FILE *eb = openFile(dir, "eb.txt", "wb");

            fwrite(&portSender, sizeof(uint16_t), 1, eb);
            fwrite(&earCap, sizeof(uint8_t), 1, eb);
            fwrite(&mouthCap, sizeof(uint8_t), 1, eb);

            fclose(eb);
            continue;
        }
        if (type == MSG_NORMAL)
        {
            if (n < 5)
                continue;

            int msgLen;
            memcpy(&msgLen, buffer + 1, sizeof(int));

            if (msgLen < 0 || msgLen > BUFLEN)
                continue;

            if (n < 5 + msgLen)
                continue;

            char message[BUFLEN + 1];

            memcpy(message, buffer + 5, msgLen);
            message[msgLen] = '\0';
            printf("NORMAL MESSAGE: %s\n", message);

            FILE *eb = openFile(dir, "eb.txt", "wb");
            fwrite(&msgLen, sizeof(int), 1, eb);
            fwrite(message, sizeof(char), msgLen, eb);

            fclose(eb);
            continue;
        }
        printf("Unknown message type: %u\n", type);
    }
    close(sockfd);
}

void sendFirstMsg(char *dir)
{
    struct sockaddr_in receiverAddr;
    FILE *bm = openFile(dir, "bm.txt", "rb");
    uint16_t SENDPORT;
    fread(&SENDPORT, sizeof(uint16_t), 1, bm);
    int sockfd = setSocketToSendData(&receiverAddr, SENDPORT);

    firstMsg fmsg;
    fread(&fmsg, sizeof(firstMsg), 1, bm);
    int n = sendto(sockfd, &fmsg, sizeof(firstMsg), 0, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr));

    printf("First message sent.....\n");

    FILE *mb = openFile(dir, "mb.txt", "wb");
    fwrite(&n, sizeof(int), 1, mb);
    fclose(mb);

    fclose(bm);
    close(sockfd);
}

void sendMsg(char *dir)
{
    struct sockaddr_in receiverAddr;
    FILE *bm = openFile(dir, "bm.txt", "rb");
    uint16_t SENDPORT;
    int len;

    fread(&SENDPORT, sizeof(uint16_t), 1, bm);
    int sockfd = setSocketToSendData(&receiverAddr, SENDPORT);
    fread(&len, sizeof(int), 1, bm);
    char buffer[BUFLEN + sizeof(int) + 1];
    buffer[0] = MSG_NORMAL;
    memcpy(buffer + 1, &len, sizeof(int));
    fread(buffer + 1 + sizeof(int), sizeof(char), len, bm);

    int packetSize = 1 + sizeof(int) + len;
    int n = sendto(sockfd, buffer, packetSize, 0, (struct sockaddr *)&receiverAddr,sizeof(receiverAddr));

    FILE *mb = openFile(dir, "mb.txt", "wb");
    fwrite(&n, sizeof(int), 1, mb);
    fclose(mb);

    fclose(bm);
    close(sockfd);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: ./executable <RECVPORT> <DIR_NAME>\n");
        exit(1);
    }
    int RECVPORT = atoi(argv[1]);
    char *dir = argv[2];
    mkdir(dir, 0777);
    createBE(dir, RECVPORT);
    printf("Starting the processes...\n");
    pid_t receiver = fork();

    if (receiver == 0)
    {
        recvMsg(dir);
        exit(0);
    }
    pid_t sender = fork();

    if (sender == 0)
    {
        takeInput(dir, first);
        FILE *eb = NULL;
        uint16_t SENDPORT;

        while ((eb = openFile(dir, "eb.txt", "rb")) == NULL){
            printf("Waiting for the first message...\n");
            sleep(5);
        }
        fread(&SENDPORT, sizeof(uint16_t), 1, eb);
        fclose(eb);
        createBM(dir, SENDPORT, first);
        sendFirstMsg(dir);

        eb = openFile(dir, "eb.txt", "rb");
        if (eb != NULL){
            fread(&first, sizeof(int), 1, eb);
            fclose(eb);
        }
        while (first == 1)
        {
            sleep(5);
            printf("Retrying to send the first message...\n");
            sendFirstMsg(dir);
            eb = openFile(dir, "eb.txt", "rb");
            if (eb != NULL)
            {
                fread(&first, sizeof(int), 1, eb);
                fclose(eb);
            }
        }
        printf("First message acknowledged.\n");
        exit(0);
    }

    waitpid(sender, NULL, 0);
    waitpid(receiver, NULL, 0);

    return 0;
}