#include <stdio.h>
#include <stdlib.h> // calloc, malloc, exit
#include <stdint.h>
#include <unistd.h> // usleep, fork, execlp
#include <string.h>
#include <sys/types.h> // pid_t
#include <sys/wait.h> // waitpid
#include <sys/stat.h> // mkdir
#include <dirent.h>
#include "./libraries/mylib.h"

#define BUFLEN 6000
#define MSG_FIRST 1
// #define MSG_ACK 2
#define MSG_NORMAL 2

Capacity anotherCap;
Capacity selfCap;
 
int sendPart(char *sourceDir, char *dir, uint8_t arr[], uint16_t len)
{
    DIR *dp = opendir(sourceDir);
    struct dirent *entry;
    if (dp == NULL){
        printf("Directory is empty\n");
        return -1;
    }

    while ((entry = readdir(dp)) != NULL){
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s",sourceDir, entry->d_name);
        FILE *part = fopen(path, "rb");
        if (part == NULL)
            continue;

        uint16_t sequence;
        if (fread(&sequence, sizeof(uint16_t), 1, part) != 1){
            fclose(part);
            continue;
        }

        // Already sent
        if (sequence == 0 || sequence > len || arr[sequence - 1] == 1){
            fclose(part);
            continue;
        }

        uint8_t buffer[BUFLEN];
        int msgLen = fread(buffer, 1, BUFLEN, part);
        fclose(part);
        if (msgLen <= 0)
            continue;

        FILE *bm = openFile(dir, "bm.txt", "wb");
        if (bm == NULL){
            closedir(dp);
            continue;
        }

        uint8_t msgType = MSG_NORMAL;
        fwrite(&msgType, sizeof(uint8_t), 1, bm);
        fwrite(&sequence, sizeof(uint16_t), 1, bm);
        fwrite(buffer, 1, msgLen, bm);
        fclose(bm);

        arr[sequence - 1] = 1;
        closedir(dp);
        printf("Message size %d\n", msgLen);
        printf("sequence number %d\n", sequence);
        return sequence;
    }
    closedir(dp);
    return -1;
}

void brain(char *dir, int SENDPORT)
{
    uint8_t receivedFirst = 0;
    uint8_t lastType = 0;
    uint16_t lastSequence = 0;
    uint8_t *arr;

    uint16_t numberOfChunks;
    char storeDir[20];
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
                    if (n < 4){
                        // printf("BRAIN: invalid FIRST packet\n");
                        usleep(1000);
                        continue;
                    }
                    if (receivedFirst){
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
                    char fileName[20];
                    printf("Enter input file : ");
                    scanf("%s", fileName);
                    printf("Enter the dir name : ");
                    scanf("%s", storeDir);

                    // split parts
                    int maxSendLen = selfCap.mouthCap >= anotherCap.earCap ? anotherCap.earCap : selfCap.mouthCap;
                    numberOfChunks = splitParts(fileName, maxSendLen, storeDir);
                    arr = calloc(numberOfChunks, sizeof(uint8_t));
                    if (sendPart(storeDir, dir, arr, numberOfChunks) == -1){
                        printf("All packet send\n");
                        return;
                    }
                    // printf("BRAIN: OUR FIRST message created\n");
                }
                else if (type == MSG_NORMAL)
                {
                    if (n < 1){
                        // printf("BRAIN: invalid NORMAL packet\n");
                        usleep(1000);
                        continue;
                    }
                    uint16_t sequence;
                    memcpy(&sequence, buffer + 1, sizeof(uint16_t));
                    // printf("\nBRAIN: NORMAL received sequence=%u\n", sequence);
                    if (lastType == MSG_NORMAL && lastSequence == sequence){
                        usleep(1000);
                        continue;
                    }

                    int msgLen = n - 1 - sizeof(uint16_t);
                    if (msgLen >= BUFLEN)
                        msgLen = BUFLEN - 1;

                    char message[BUFLEN];
                    memcpy(message, buffer + 5, msgLen);
                    message[msgLen] = '\0';

                    // printf("BRAIN: message = %s\n", message);
                   
                    lastType = MSG_NORMAL;
                    lastSequence = sequence;
                    FILE *bm = openFile(dir, "bm.txt", "wb");
                    if (bm != NULL)
                    {
                        if (sendPart(storeDir, dir, arr, numberOfChunks) == -1){
                            printf("All packet send\n");
                            fflush(stdout);
                            FILE *bm = openFile(dir, "bm.txt", "wb");
                            uint8_t normal = MSG_NORMAL;
                            uint16_t seq = UINT16_MAX;
                            fwrite(&normal, sizeof(uint8_t), 1, bm);
                            fwrite(&seq, sizeof(uint16_t), 1, bm);
                            char *msg = "this is end\n";
                            fwrite(msg, sizeof(char), sizeof(msg), bm);
                            fclose(bm);
                            exit(0);
                        }
                        printf("BRAIN: next msg send\n");
                    }
                }
                else
                {
                    printf("BRAIN: unknown message type %u\n", type);
                }
            }
        }
        usleep(1000);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 4){
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
        execlp("./ear","./ear", dir, NULL);
        exit(0);
    }
    brain(dir, SENDPORT);

    waitpid(sender, NULL, 0);
    waitpid(receiver, NULL, 0);
    return 0;
}
