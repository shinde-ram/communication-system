
#include <stdio.h>
#ifndef FUNCTION_H
#define FUNCTION_H

FILE *openFile(const char *dir, const char *filename, const char *mode);

void takeInput(char *dir, int first);

void createBE(char *dir, int RECVPORT);

void createBM(char *dir, uint16_t SENDPORT, int first);

int setSocketToSendData(struct sockaddr_in *receiverAddr, uint16_t SENDPORT);
#endif