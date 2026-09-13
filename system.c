#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include "./libraries/mylib.h"

#define BUFLEN 6000
#define MSG_FIRST 1
#define MSG_NORMAL 2
#define MSG_ACK 3
#define MSG_META 4

Capacity anotherCap;
Capacity selfCap;

char destDir[30];

int readEB(char *dir, uint8_t buffer[], int *n)
{
    FILE *eb = openFile(dir, "eb.txt", "rb");
    if (eb == NULL)
        return 0;

    *n = fread(buffer, 1, BUFLEN, eb);
    fclose(eb);

    clearEB(dir);
    if (*n <= 0)
        return 0;
    return 1;
}

// SEND MISSING PARTS
void sendMissingList(char *dir, uint8_t *arrRecv, uint16_t parts, uint16_t *packetSequence)
{
    uint16_t offset = 0;
    // printf("in the missing function\n");
    while (offset < parts)
    {
        FILE *bm = openFile(dir, "bm.txt", "wb");
        if (bm == NULL)
        {
            usleep(10000);
            continue;
        }

        uint8_t type = MSG_NORMAL;
        uint16_t sequencePacket = *packetSequence;
        fwrite(&type, sizeof(uint8_t), 1, bm);
        fwrite(&sequencePacket, sizeof(uint16_t), 1, bm);

        int count = 0;
        while (offset < parts)
        {
            uint8_t min = anotherCap.earCap < selfCap.mouthCap ? anotherCap.earCap : selfCap.mouthCap;
            if (3 + ((count + 1) * sizeof(uint16_t)) > min)
                break;

            if (arrRecv[offset] == 0)
            {
                uint16_t missingSequence = offset + 1;
                fwrite(&missingSequence, sizeof(uint16_t), 1, bm);
                count++;
            }
            offset++;
        }
        fclose(bm);
        // printf("SYSTEM: Missing-list packet = %u, missing count = %d\n", sequencePacket, count);
        (*packetSequence)++;
        if (count == 0)
            break;
        usleep(10000);
    }
}

// CHECK IF ALL PARTS RECEIVED
int allPartsReceived(uint8_t *arrRecv, uint16_t parts)
{
    for (uint16_t i = 0; i < parts; i++)
    {
        if (arrRecv[i] == 0)
            return 0;
    }
    return 1;
}

void brain(char *dir)
{
    uint8_t receivedFirst = 0;
    uint8_t firstSent = 0;
    uint8_t connected = 0;

    uint16_t parts = 0;
    uint8_t *arrRecv = NULL;

    uint16_t packetSequence = 1;
    static int noChangeCount = 0;
    uint8_t waitingForUser = 0;
    char fileName[256];

    printf("BRAIN: started\n");
    while (1)
    {
        uint8_t buffer[BUFLEN];
        int n = 0;
        int received = readEB(dir, buffer, &n);

        if (received)
        {
            noChangeCount = 0;
            uint8_t type = buffer[0];
            if (type == MSG_FIRST)
            {

                uint8_t type = buffer[0];
                if (n < 7 || receivedFirst)
                {
                    usleep(10000);
                    continue;
                }

                memcpy(&anotherCap.port, buffer + 3, sizeof(uint16_t));
                memcpy(&anotherCap.earCap, buffer + 5, sizeof(uint8_t));
                memcpy(&anotherCap.mouthCap, buffer + 6, sizeof(uint8_t));

                printf("\n");
                printf("BRAIN: FIRST received\n");
                receivedFirst = 1;

                takeInput(dir, 1, &selfCap);
                createBM(dir, anotherCap.port, 1);
                firstSent = 1;
            }
            else if (type == MSG_META)
            {
                if (n < 4)
                {
                    usleep(10000);
                    continue;
                }

                uint8_t nameSize;
                memcpy(&nameSize, buffer + 1, sizeof(uint8_t));
                if (n < 2 + nameSize + sizeof(uint16_t))
                {
                    usleep(10000);
                    continue;
                }

                if (nameSize >= sizeof(fileName))
                    nameSize = sizeof(fileName) - 1;

                memcpy(fileName, buffer + 2, nameSize);
                fileName[nameSize] = '\0';
                memcpy(&parts, buffer + 2 + nameSize, sizeof(uint16_t));

                printf("\n");
                printf("BRAIN: META received\n");
                printf("File name : %s\n", fileName);
                printf("Number of parts : %u\n", parts);

                free(arrRecv);
                arrRecv = calloc(parts, sizeof(uint8_t));

                FILE *bm = openFile(dir, "bm.txt", "wb");
                if (bm != NULL)
                {
                    uint8_t responseType = MSG_META;
                    fwrite(&responseType, sizeof(uint8_t), 1, bm);
                    fclose(bm);
                }
                packetSequence = 1;
                noChangeCount = 0;
            }
            else if (type == MSG_NORMAL)
            {
                if (n < 3)
                {
                    usleep(10000);
                    continue;
                }

                clearBM(dir);

                uint16_t sequence;
                memcpy(&sequence, buffer + 1, sizeof(uint16_t));

                if (sequence == 0 || sequence > parts || arrRecv == NULL)
                {
                    usleep(100000);
                    continue;
                }

                if (!connected && receivedFirst && firstSent)
                {
                    printf("Enter folder name\n");
                    scanf("%29s", destDir);

                    char *cmd = malloc(strlen(destDir) + strlen("rm -rf") + 3);
                    sprintf(cmd, "rm -rf \"%s\"", destDir);
                    system(cmd);
                    free(cmd);
                    mkdir(destDir, 0777);

                    connected = 1;
                    printf("\n");
                    printf("BRAIN: CONNECTION ESTABLISHED\n");
                }

                int msgLen = n - 3;
                if (msgLen <= 0)
                {
                    usleep(10000);
                    continue;
                }
                addFileInFolder(destDir, sequence, buffer + 3, msgLen);
                arrRecv[sequence - 1] = 1;
                // printf("BRAIN: NORMAL received sequence=%u\n", sequence);
                noChangeCount = 0;
            }

            else
            {
                printf("BRAIN: unknown message type %u\n", type);
            }
        }

        if (connected && arrRecv != NULL && parts > 0)
        {
            noChangeCount++;

            if (noChangeCount >= 100)
            {
                printf("\n");
                printf("BRAIN: Transmission round finished\n");

                if (allPartsReceived(arrRecv, parts))
                {
                    printf("BRAIN: All parts received\n");
                    FILE *bm = openFile(dir, "bm.txt", "wb");
                    if (bm != NULL)
                    {
                        uint8_t responseType = MSG_NORMAL;
                        fwrite(&responseType, sizeof(uint8_t), 1, bm);
                        fwrite(&packetSequence, sizeof(uint16_t), 1, bm);
                        fclose(bm);
                        packetSequence++;
                    }
                    usleep(10000);
                    clearBM(dir);

                    noChangeCount = 0;
                    combineCurrentFiles(destDir, fileName);
                    return;
                }
                else
                {
                    printf("BRAIN: Missing parts found\n");

                    sendMissingList(dir, arrRecv, parts, &packetSequence);
                    usleep(1000);

                    clearBM(dir);
                    noChangeCount = 0;
                }
            }
        }
        usleep(10000);
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

    char *cmd = malloc(strlen(dir) + strlen("rm -rf") + 3);
    sprintf(cmd, "rm -rf \"%s\"", dir);
    system(cmd);
    free(cmd);
    mkdir(dir, 0777);

    createBE(dir, RECVPORT);

    printf("Starting the processes...\n");

    pid_t receiver = fork();
    if (receiver == 0)
    {
        execlp("./ear", "./ear", dir, NULL);
        exit(0);
    }

    pid_t sender = fork();
    if (sender == 0)
    {
        execlp("./mouth", "./mouth", dir, NULL);
        exit(0);
    }

    brain(dir);

    kill(sender, SIGTERM);
    kill(receiver, SIGTERM);

    waitpid(sender, NULL, 0);
    waitpid(receiver, NULL, 0);
    return 0;
}