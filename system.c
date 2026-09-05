#include <stdio.h> 
#include <stdlib.h> // malloc, free, atoi, exit, system
#include <unistd.h> // usleep, fork, execlp
#include <stdint.h>
#include <string.h> // memcpy, strlen
#include <sys/types.h> // pid_t
#include <sys/wait.h> // waitpid
#include <sys/stat.h> // mkdir
#include "./libraries/mylib.h"

#define BUFLEN 6000
#define MSG_FIRST 1
// #define MSG_ACK 2
#define MSG_NORMAL 2

Capacity anotherCap;
Capacity selfCap;

char destDir[30];

void brain(char *dir)
{
    uint8_t receivedFirst = 0;
    uint8_t firstSent = 0;
    uint8_t connected = 0;

    uint8_t lastType = 0;
    uint16_t lastSequence = 0;

    uint16_t totalSeq;
    char *fileName;

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
                        usleep(1000);
                        continue;
                    }
                    if (receivedFirst)
                    {
                        // printf("BRAIN: duplicate FIRST received\n");
                        usleep(1000);
                        continue;
                    }

                    memcpy(&anotherCap.port, buffer + 3, sizeof(uint16_t));
                    memcpy(&anotherCap.earCap, buffer + 5, sizeof(uint8_t));
                    memcpy(&anotherCap.mouthCap, buffer + 6, sizeof(uint8_t));

                    printf("\n");
                    printf("BRAIN: FIRST received\n");
                    // printf("Other SENDPORT = %u\n", anotherCap.port);
                    // printf("Other EAR CAP = %u\n", anotherCap.earCap);
                    // printf("Other MOUTH CAP  = %u\n", anotherCap.mouthCap);

                    receivedFirst = 1;

                    takeInput(dir, 1, &selfCap);
                    createBM(dir, anotherCap.port, 1);

                    firstSent = 1;
                    // printf("BRAIN: OUR FIRST message created\n");
                }
                else if (type == MSG_NORMAL)
                {
                    if (n < 5)
                    {
                        // printf("BRAIN: invalid NORMAL packet\n");
                        usleep(1000);
                        continue;
                    }

                    uint16_t sequence;
                    memcpy(&sequence, buffer + 1, sizeof(uint16_t));
                    if (sequence == UINT16_MAX)
                    {
                        combineFile(destDir);
                        printf("All files received. Combined\n");
                        exit(0);
                    }
                    
                    if (lastType == MSG_NORMAL && lastSequence == sequence)
                    {
                        // printf("BRAIN: duplicate sequence %u\n",sequence);
                        FILE *bm = openFile(dir, "bm.txt", "wb");
                        if (bm != NULL)
                        {
                            uint8_t ackType = MSG_NORMAL;
                            fwrite(&ackType, sizeof(uint8_t), 1, bm);
                            fwrite(&sequence, sizeof(uint16_t), 1, bm);
                            fclose(bm);
                            // printf("BRAIN: ACK resent for sequence %u\n", sequence);
                        }
                        usleep(1000);
                        continue;
                    }
                    
                    printf("\nBRAIN: NORMAL received sequence=%u\n", sequence);
                    int msgLen = n - 1 - sizeof(uint16_t);
                    if (msgLen >= BUFLEN)
                        msgLen = BUFLEN - 1;

                    char message[BUFLEN];
                    memcpy(message, buffer + 3, msgLen);
                    message[msgLen] = '\0';

                    // printf("BRAIN: message length = %d\n", msgLen);
                    if (!connected && receivedFirst && firstSent)
                    {

                        printf("Enter folder name\n");
                        scanf("%19s", destDir);
                        char *cmd = malloc(strlen(destDir) + 2 + strlen("rm -rf"));
                        sprintf(cmd, "rm -rf \"%s\"", destDir);
                        system(cmd);
                        mkdir(destDir, 0777);
                        free(cmd);

                        connected = 1;
                        printf("\n");
                        // printf(" BRAIN: CONNECTION ESTABLISHED\n");
                    }
                    addFileInFolder(destDir, sequence, buffer + 3, msgLen);

                    lastType = MSG_NORMAL;
                    lastSequence = sequence;
                    FILE *bm = openFile(dir, "bm.txt", "wb");
                    if (bm != NULL)
                    {
                        uint8_t ackType = MSG_NORMAL;
                        fwrite(&ackType, sizeof(uint8_t), 1, bm);
                        fwrite(&sequence, sizeof(uint16_t), 1, bm);
                        fclose(bm);

                        // printf("BRAIN: ACK created for sequence %u\n", sequence);
                    }
                }
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

        usleep(1000);
    }
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
        execlp("./ear","./ear", dir, NULL);
        exit(0);
    }

    pid_t sender = fork();
    if (sender == 0)
    {
        execlp("./mouth", "./mouth", dir, NULL);
        exit(0);
    }

    brain(dir);
    waitpid(sender, NULL, 0);
    waitpid(receiver, NULL, 0);
    return 0;
}
