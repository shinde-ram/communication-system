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
// #define MSG_ACK 2
#define MSG_NORMAL 2

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

typedef struct anotherCapability
{
    uint16_t port;
    uint8_t earCap;
    uint8_t mouthCap;
} Acap;

Acap acap;
firstMsg fmsg;

void writeOutput(char *dir)
{
    int n;
    char buffer[BUFLEN];

    firstMsg *fmsgs = malloc(sizeof(firstMsg));
    FILE *eb = openFile(dir, "eb.txt", "rb");
    if (eb == NULL)
    {
        perror("File not created yet\n");
        exit(0);
    }
    fread(&n, sizeof(int), 1, eb);
    fread(buffer, sizeof(char), n, eb);
    fclose(eb);

    FILE *outputUI = openFile(dir, "outputUI.txt", "wb");
    fwrite(&n, sizeof(int), 1, outputUI);
    fwrite(buffer, sizeof(char), n, outputUI);
    fclose(outputUI);
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

// void sendFirstMsg(char *dir)
// {
//     struct sockaddr_in receiverAddr;
//     FILE *bm = openFile(dir, "bm.txt", "rb");
//     uint16_t SENDPORT;
//     fread(&SENDPORT, sizeof(uint16_t), 1, bm);
//     int sockfd = setSocketToSendData(&receiverAddr, SENDPORT);
//     firstMsg fmsg;
//     fread(&fmsg, sizeof(firstMsg), 1, bm);
//     int n = sendto(sockfd, &fmsg, sizeof(firstMsg), 0, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr));
//     printf("First message sent.....\n");
//     FILE *mb = openFile(dir, "mb.txt", "wb");
//     fwrite(&n, sizeof(int), 1, mb);
//     fclose(mb);
//     fclose(bm);
//     close(sockfd);
// }

void sendMsg(char *dir)
{
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
        sleep(1);
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

        if(msgType == MSG_FIRST && fread(&SENDPORT, sizeof(uint16_t), 1, bm) != 1){
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
            sleep(1);
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

void brain(char *dir, int SENDPORT)
{
    uint8_t receivedFirst = 0;
    uint8_t firstSent = 0;
    uint8_t connected = 0;

    uint8_t lastType = 0;
    uint16_t lastSequence = 0;
    uint16_t sendSequence = 1;

    uint16_t waitingAck = 0;

    int waitingForAck = 0;

    printf("BRAIN: started\n");
    while (1)
    {

        FILE *eb = openFile(dir, "eb.txt", "rb");

        if (eb != NULL)
        {
            uint8_t buffer[BUFLEN];

            int n = fread(buffer, 1, BUFLEN, eb);
            fclose(eb);
            if (n > 0)
            {
                uint8_t type = buffer[0];
                if (type == MSG_FIRST)
                {
                    if (n < 4)
                    {
                        // printf("BRAIN: invalid FIRST packet\n");
                        sleep(1);
                        continue;
                    }
                    if (receivedFirst)
                    {
                        // printf("BRAIN: duplicate FIRST received\n");
                        sleep(1);
                        continue;
                    }

                    memcpy(&acap.port, buffer + 3, sizeof(uint16_t));
                    memcpy(&acap.earCap, buffer + 5, sizeof(uint8_t));
                    memcpy(&acap.mouthCap, buffer + 6, sizeof(uint8_t));

                    printf("\n");
                    printf("BRAIN: FIRST received\n");
                    printf("Other SENDPORT = %u\n", acap.port);
                    printf("Other EAR CAP = %u\n", acap.earCap);
                    printf("Other MOUTH CAP  = %u\n", acap.mouthCap);

                    receivedFirst = 1;

                    takeInput(dir, 2);
                    createBM(dir, SENDPORT, 2);


                    firstSent = 1;
                    // printf("BRAIN: OUR FIRST message created\n");
                }
                else if (type == MSG_NORMAL)
                {
                    if (n < 1)
                    {
                        // printf("BRAIN: invalid NORMAL packet\n");
                        sleep(1);
                        continue;
                    }

                    uint16_t sequence;
                    memcpy(&sequence, buffer + 1, sizeof(uint16_t));

                    // printf("\nBRAIN: NORMAL received sequence=%u\n", sequence);
                    if (lastType == MSG_NORMAL && lastSequence == sequence)
                    {
                        // printf("BRAIN: duplicate sequence %u\n", sequence);
                        // FILE *bm = openFile(dir, "bm.txt", "wb");
                        // if (bm != NULL)
                        // {
                        //     uint8_t ackType = MSG_NORMAL;
                        //     fwrite(&ackType, sizeof(uint8_t), 1, bm);
                        //     fwrite(&sequence, sizeof(uint16_t), 1, bm);
                        //     fclose(bm);
                        //     printf("BRAIN: ACK resent for sequence %u\n", sequence);
                        // }
                        sleep(1);
                        continue;
                    }

                    int msgLen = n - 1 - sizeof(uint16_t);
                    if (msgLen >= BUFLEN)
                        msgLen = BUFLEN - 1;

                    char message[BUFLEN];
                    memcpy(message, buffer + 5, msgLen);
                    message[msgLen] = '\0';

                    // printf("BRAIN: message = %s\n", message);
                    if (!connected && receivedFirst && firstSent)
                    {
                        connected = 1;
                        printf("\n");
                        // printf(" BRAIN: CONNECTION ESTABLISHED\n");
                    }
                    lastType = MSG_NORMAL;
                    lastSequence = sequence;
                    FILE *bm = openFile(dir, "bm.txt", "wb");
                    if (bm != NULL)
                    {
                        // uint8_t ackType = MSG_ACK;
                        // fwrite(&ackType, sizeof(uint8_t), 1, bm);
                        // fwrite(&sequence, sizeof(uint16_t), 1, bm);
                        // fclose(bm);
                        takeInput(dir, 2);
                        createBM(dir, SENDPORT, 2);

                        // printf("BRAIN: next msg send");
                    }
                }
                // else if (type == MSG_ACK)
                // {
                //     if (n < 5)
                //     {
                //         printf("BRAIN: invalid ACK packet\n");
                //         continue;
                //     }

                //     uint32_t ackSequence;
                //     memcpy(&ackSequence, buffer + 1, sizeof(uint32_t));
                //     printf("BRAIN: ACK received for sequence %u\n", ackSequence);
                //     if (waitingForAck && ackSequence == waitingAck)
                //     {
                //         waitingForAck = 0;
                //         printf("BRAIN: sequence %u successfully delivered\n",ackSequence);
                //     }
                // }
                else
                {
                    printf("BRAIN: unknown message type %u\n", type);
                }
            }
        }
        if (connected)
        {
            // printf("Connected\n");
        }

        sleep(1);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("Usage: ./executable <SENDPORT> <RECVPORT> <DIR_NAME>\n");
        exit(1);
    }
    int SENDPORT = atoi(argv[1]);
    int RECVPORT = atoi(argv[2]);
    char *dir = argv[3];

    // Remove and recreate dir
    char *cmd = malloc(sizeof(dir) + 2 + strlen("rm -rf"));
    sprintf(cmd, "rm -rf \"%s\"", dir);
    system(cmd);
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
        takeInput(dir, 1);
        createBM(dir, SENDPORT, 1);
        sendMsg(dir);
        exit(0);
    }

    brain(dir, SENDPORT);

    waitpid(sender, NULL, 0);
    waitpid(receiver, NULL, 0);

    return 0;
}
