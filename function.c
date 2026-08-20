#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "function.h"

#define BUFLEN 1024

FILE *openFile(const char *dir, const char *filename, const char *mode)
{
	char *path = malloc(strlen(dir) + strlen(filename) + 2);
	sprintf(path, "%s/%s", dir, filename);
	FILE *file = fopen(path, mode);
	free(path);
	// printf("File %s opened in mode %s\n", filename, mode);
	return file;
}

void takeInput(char *dir, int first)
{
	FILE *inputUI = openFile(dir, "inputUI.txt", "wb");
	if (inputUI == NULL)
	{
		perror("Failed to create inputUI file");
		exit(EXIT_FAILURE);
	}
	if (first == 1)
	{
		uint16_t port;
		uint8_t earCap;
		uint8_t mouthCap;
		unsigned int temp;
		printf("Enter port number : ");
		scanf("%d", &temp);
		port = (uint16_t)temp;

		printf("Enter ear capacity : ");
		scanf("%d", &temp);
		earCap = (uint8_t)temp;

		printf("Enter mouth capacity : ");
		scanf("%d", &temp);
		mouthCap = (uint8_t)temp;

		fwrite(&port, sizeof(uint16_t), 1, inputUI);
		fwrite(&earCap, sizeof(uint8_t), 1, inputUI);
		fwrite(&mouthCap, sizeof(uint8_t), 1, inputUI);
		fclose(inputUI);
	}
	else
	{
		char *messagestr = malloc(BUFLEN);
		printf("Enter the message to send:: ");
		// fgets(messagestr, BUFLEN, stdin);
		scanf(" %[^\n]s", messagestr); // Read until newline
		int len = strlen(messagestr);
		FILE *inputUI = openFile(dir, "inputUI.txt", "wb");
		if (inputUI == NULL)
		{
			perror("Failed to create inputUI file");
			exit(EXIT_FAILURE);
		}
		fwrite(&len, sizeof(int), 1, inputUI);
		fwrite(messagestr, sizeof(char), len, inputUI);
		fclose(inputUI);
		free(messagestr);
		printf("msg taken\n");
	}
}

void createBE(char *dir, int port)
{
	FILE *be = openFile(dir, "be.txt", "wb");
	if (be == NULL)
	{
		perror("Failed to create BE file");
		exit(EXIT_FAILURE);
	}
	fwrite(&port, sizeof(int), 1, be);
	fclose(be);
}

void createBM(char *dir, uint16_t SENDPORT, int first)
{
	FILE *bm = openFile(dir, "bm.txt", "wb");
	FILE *inputUI = openFile(dir, "inputUI.txt", "rb");

	if (first == 1)
	{
		uint16_t portSender;
		uint8_t earCap;
		uint8_t mouthCap;
		fread(&portSender, sizeof(uint16_t), 1, inputUI);
		fread(&earCap, sizeof(uint8_t), 1, inputUI);
		fread(&mouthCap, sizeof(uint8_t), 1, inputUI);
		uint8_t first = 1;
		fwrite(&SENDPORT, sizeof(uint16_t), 1, bm);
		fwrite(&first, sizeof(uint8_t), 1, bm);
		fwrite(&portSender, sizeof(uint16_t), 1, bm);
		fwrite(&earCap, sizeof(uint8_t), 1, bm);
		fwrite(&mouthCap, sizeof(uint8_t), 1, bm);
	}
	else
	{
		int len;
		fread(&len, sizeof(int), 1, inputUI);
		char *messageStr = malloc(len + 1);
		fread(messageStr, sizeof(char), len, inputUI);

		fwrite(&SENDPORT, sizeof(uint16_t), 1, bm);
		fwrite(&len, sizeof(int), 1, bm);
		fwrite(messageStr, sizeof(char), len, bm);
		free(messageStr);
	}
	fclose(inputUI);
	fclose(bm);
}

int setSocketToSendData(struct sockaddr_in *receiverAddr, uint16_t SENDPORT)
{
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}
	receiverAddr->sin_family = AF_INET;
	receiverAddr->sin_port = htons(SENDPORT);
	receiverAddr->sin_addr.s_addr = INADDR_ANY;

	return sockfd;
}