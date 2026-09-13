#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include "./libraries/mylib.h"

#define BUFLEN 6000
#define MSG_FIRST 1
#define MSG_NORMAL 2
#define MSG_ACK 3
#define MSG_META 4
#define WAIT_COUNT 10

Capacity anotherCap;
Capacity selfCap;

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

//SEND FILE PARTS
void sendParts(char *sourceDir, char *dir, uint8_t arr[], uint16_t len)
{
    DIR *dp = opendir(sourceDir);
    if (dp == NULL){
        printf("Directory is empty\n");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", sourceDir, entry->d_name);

        FILE *part = fopen(path, "rb");
        if (part == NULL)
            continue;

        uint16_t sequence;
        if (fread(&sequence, sizeof(uint16_t), 1, part) != 1 && (sequence == 0 || sequence > len || arr[sequence - 1] == 0)){
            fclose(part);
            continue;
        }

        uint8_t buffer[BUFLEN];
        int msgLen = fread(buffer, 1, BUFLEN, part);
        fclose(part);

        if (msgLen <= 0)
            continue;

        FILE *bm = openFile(dir, "bm.txt", "wb");
        if (bm == NULL)
            continue;

        uint8_t msgType = MSG_NORMAL;
        fwrite(&msgType, sizeof(uint8_t), 1, bm);
        fwrite(&sequence, sizeof(uint16_t), 1, bm);
        fwrite(buffer, 1, msgLen, bm);
        fclose(bm);
        // printf("Message size : %d\n", msgLen);
        // printf("Sequence number : %u\n\n", sequence);
        usleep(10000);
    }
    closedir(dp);
}

//CREATE META
void createMeta(char *dir, char *fileName, uint16_t numberOfChunks)
{
    FILE *bm = openFile(dir, "bm.txt", "wb");
    if (bm == NULL)
        return;

    uint8_t msgType = MSG_META;
    uint8_t nameSize = strlen(fileName);

    fwrite(&msgType, sizeof(uint8_t), 1, bm);
    fwrite(&nameSize, sizeof(uint8_t), 1, bm);
    fwrite(fileName, sizeof(char), nameSize, bm);
    fwrite(&numberOfChunks, sizeof(uint16_t), 1, bm);
    fclose(bm);
}

//HANDLE FIRST
void handleFirst(char *dir, uint8_t *receivedFirst, uint16_t *numberOfChunks, char *storeDir)
{
    char fileName[20]; 
    printf("\n");
    printf("BRAIN: FIRST received\n");
    printf("Other SENDPORT = %u\n", anotherCap.port);
    printf("Other EAR CAP = %u\n", anotherCap.earCap);
    printf("Other MOUTH CAP = %u\n", anotherCap.mouthCap);

    *receivedFirst = 1;

    printf("Enter input file : ");
    scanf("%19s", fileName);

    printf("Enter the dir name : ");
    scanf("%19s", storeDir);

    int maxSendLen = selfCap.mouthCap >= anotherCap.earCap ? anotherCap.earCap : selfCap.mouthCap;
    *numberOfChunks = splitParts(fileName, maxSendLen, storeDir);
    printf("Number of parts : %u\n", *numberOfChunks);
    createMeta(dir, fileName, *numberOfChunks);
}

//HANDLE MISSING LIST
int handleMissingList(uint8_t buffer[], int n, uint16_t numberOfChunks, uint8_t arrResend[])
{
    if (n <= 3){
        printf("BRAIN: No missing parts\n");
        return 1;
    }

    uint16_t sequencePacket;
    memcpy(&sequencePacket, buffer + 1, sizeof(uint16_t));

    printf("BRAIN: New missing list packet = %u\n", sequencePacket);

    int offset = 3;
    while (offset + sizeof(uint16_t) <= n)
    {
        uint16_t missingSequence;
        memcpy(&missingSequence, buffer + offset, sizeof(uint16_t));

        if (missingSequence > 0 && missingSequence <= numberOfChunks)
        {
            arrResend[missingSequence - 1] = 1;
            // printf("Missing sequence = %u\n", missingSequence);
        }
        offset += sizeof(uint16_t);
    }
    return 0;
}

//BRAIN
void brain(char *dir, int SENDPORT)
{
    uint8_t receivedFirst = 0;
    uint16_t numberOfChunks = 0;
    char storeDir[20];
    uint8_t *arrResend = NULL;
    uint8_t receivingMissingList = 0;
    int noPacketCount = 0;

    printf("BRAIN: started\n");
    while (1)
    {
        uint8_t buffer[BUFLEN];
        int n = 0;
        int received = readEB(dir, buffer, &n);

        if (received)
        {
            noPacketCount = 0;
            uint8_t type = buffer[0];

            if (type == MSG_FIRST)
            {
                if (n < 7 || receivedFirst)
                    continue;

                memcpy(&anotherCap.port, buffer + 3, sizeof(uint16_t));
                memcpy(&anotherCap.earCap, buffer + 5, sizeof(uint8_t));
                memcpy(&anotherCap.mouthCap, buffer + 6, sizeof(uint8_t));
                handleFirst(dir, &receivedFirst, &numberOfChunks, storeDir);
            }

            else if (type == MSG_META){
                free(arrResend);
                arrResend = malloc(numberOfChunks * sizeof(uint8_t));
                if (arrResend == NULL){
                    printf("ERROR: malloc failed\n");
                    continue;
                }
                printf("META : this is the meta\n");

                memset(arrResend, 1, numberOfChunks);
                sendParts(storeDir, dir, arrResend, numberOfChunks);
                memset(arrResend, 0, numberOfChunks);

                clearBM(dir);
                receivingMissingList = 0;
                noPacketCount = 0;
            }
            else if (type == MSG_NORMAL){
                if (n < 3)
                    continue;

                int noMissing = handleMissingList(buffer, n, numberOfChunks, arrResend);
                if (noMissing)
                {
                    printf("BRAIN: Transfer completed\n");
                    clearBM(dir);
                    free(arrResend);
                    return;
                }
                receivingMissingList = 1;
                noPacketCount = 0;
            }
            else
            {
                printf("BRAIN: unknown message type %u\n", type);
            }
        }
        else
        {
            if (receivingMissingList)
                noPacketCount++;
        }

        if (receivingMissingList && noPacketCount >= WAIT_COUNT)
        {
            printf("\n");
            printf("BRAIN: Missing-list batch finished\n");
            printf("BRAIN: Sending requested parts\n");

            sendParts(storeDir, dir, arrResend, numberOfChunks);
            memset(arrResend, 0, numberOfChunks);

            receivingMissingList = 0;
            noPacketCount = 0;
            clearBM(dir);
        }
        usleep(10000);
    }
}

//MAIN
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

    char *cmd = malloc(strlen(dir) + strlen("rm -rf") + 3);
    sprintf(cmd, "rm -rf \"%s\"", dir);
    system(cmd);
    free(cmd);
    mkdir(dir, 0777);

    printf("Starting the processes...\n");

    createBE(dir, RECVPORT);
    takeInput(dir, 1, &selfCap);
    createBM(dir, SENDPORT, 1);

    pid_t receiver = fork();
    if (receiver == 0)
    {
        execlp("./mouth", "./mouth", dir, NULL);
        exit(0);
    }

    pid_t sender = fork();
    if (sender == 0)
    {
        execlp("./ear", "./ear", dir, NULL);
        exit(0);
    }

    brain(dir, SENDPORT);

    kill(sender, SIGTERM);  
    kill(receiver, SIGTERM);

    waitpid(sender, NULL, 0);
    waitpid(receiver, NULL, 0);
    return 0;
}