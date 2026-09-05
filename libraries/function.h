
#ifndef FUNCTION_H
#define FUNCTION_H

#include <netinet/in.h>

typedef struct capacity
{
    uint16_t port;
    uint8_t earCap;
    uint8_t mouthCap;
} Capacity;

FILE *openFile(const char *dir, const char *filename, const char *mode);

void takeInput(char *dir, int first, Capacity *capacity );

void createBE(char *dir, int RECVPORT);

void createBM(char *dir, uint16_t SENDPORT, int first);

int setSocketToSendData(struct sockaddr_in *receiverAddr, uint16_t SENDPORT);
#endif