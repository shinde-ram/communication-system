#include <stdio.h> // FILE, fread, fclose, fseek
#include <stdint.h> // uint8_t, uint16_t
#include <stdlib.h> // exit
#include <unistd.h> // usleep, close
#include <sys/socket.h>  // sendto
#include <netinet/in.h>  // struct sockaddr_in

#include "./libraries/mylib.h"

#define BUFLEN 6000
#define MSG_FIRST 1
#define MSG_NORMAL 2

int main(int argc, char *argv[]){
    if(argc < 2){
        perror("MOUTH : give the dir : ");
        exit(0);
    }
    
    char *dir = argv[1];

    char buffer[BUFLEN];

    uint16_t SENDPORT;
    uint8_t msgType;
    uint16_t sequence;

    uint8_t lastType = 0;
    uint16_t lastSequence = 0;
    int hasLastMessage = 0;

    struct sockaddr_in receiverAddr;
    FILE *bm = openFile(dir, "bm.txt", "rb");

    while (bm == NULL)
    {
        usleep(1000);
        bm = openFile(dir, "bm.txt", "rb");
    }

    fread(&msgType, sizeof(uint8_t), 1, bm);
    fread(&SENDPORT, sizeof(uint16_t), 1, bm);
    fclose(bm);

    // printf("MOUTH: SENDPORT = %u\n", SENDPORT);
    int sockfd = setSocketToSendData(&receiverAddr, SENDPORT);
    while (1)
    {
        bm = openFile(dir, "bm.txt", "rb");
        if (bm == NULL)
            continue;

        if (fread(&msgType, sizeof(uint8_t), 1, bm) != 1)
        {
            fclose(bm);
            continue;
        }

        if (msgType == MSG_FIRST && fread(&SENDPORT, sizeof(uint16_t), 1, bm) != 1)
        {
            fclose(bm);
            continue;
        }

        if (msgType != MSG_FIRST && fread(&sequence, sizeof(uint16_t), 1, bm) != 1)
        {
            fclose(bm);
            continue;
        }

        if (hasLastMessage && sequence == lastSequence)
        {
            fclose(bm);
            // printf("MOUTH: Duplicate wait 3 sec\n");
            usleep(5000);
            continue;
        }

        fseek(bm, 0, SEEK_SET);

        int msgLen = fread(buffer, 1, BUFLEN, bm);
        fclose(bm);
        if (msgLen <= 0)
            continue;

        int n = sendto(sockfd, buffer, msgLen, 0, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr));
        // printf("MOUTH: type=%u seq=%u sent=%d bytes\n", msgType, sequence, n);

        lastType = msgType;
        lastSequence = sequence;
        hasLastMessage = 1;
    }
    close(sockfd);
}