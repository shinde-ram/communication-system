#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include "./libraries/mylib.h"

#define BUFLEN 6000
#define MSG_FIRST 1
#define MSG_NORMAL 2

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        perror("EAR : give the dir : ");
        exit(0);
    }
    
    char *dir = argv[1];
    struct sockaddr_in senderAddr, receiverAddr;
    uint8_t buffer[BUFLEN];

    memset(&receiverAddr, 0, sizeof(receiverAddr));
    memset(&senderAddr, 0, sizeof(senderAddr));

    FILE *be = openFile(dir, "be.txt", "rb");

    int RECVPORT;
    fread(&RECVPORT, sizeof(int), 1, be);
    fclose(be);

    int sockfd = setSocketToSendData(&receiverAddr, RECVPORT);

    if (bind(sockfd, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr)) < 0)
    {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // printf("EAR: listening on port %d\n", RECVPORT);
    while (1)
    {
        socklen_t len = sizeof(senderAddr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&senderAddr, &len);

        if (n <= 0)
            continue;

        /* EAR does not interpret the message.
           It simply gives the raw packet to BRAIN. */

        FILE *eb = openFile(dir, "eb.txt", "wb");
        fwrite(buffer, sizeof(uint8_t), n, eb);
        fclose(eb);
        // printf("EAR: received %d bytes\n", n);
    }
    close(sockfd);
}