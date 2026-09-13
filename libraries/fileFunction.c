#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "fileFunction.h"

#define BUFLEN 1024

int splitParts(char *fileName, int mouthCap, char *dir)
{
    char *cmd = malloc(strlen(dir) + 2 + strlen("rm -rf"));
    sprintf(cmd, "rm -rf \"%s\"", dir);
    system(cmd);
    mkdir(dir, 0777);

    FILE *fp = fopen(fileName, "rb");
    if (fp == NULL)
    {
        printf("File not opening\n");
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    int totalSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    int parts = totalSize / (mouthCap - 3); // 1 byte for msgType, 2 byte for sequence number
    if (totalSize % (mouthCap - 3) > 0)
        parts++;
    int partsLen = mouthCap - 3;

    char buffer[partsLen];
    char f[30];

    for (int i = 1; i <= parts; i++)
    {
        sprintf(f, "%s/file%d.txt", dir, i);
        FILE *wf = fopen(f, "wb");
        if (wf == NULL)
            continue;

        fwrite(&i, sizeof(uint16_t), 1, wf);
        size_t bytesRead;
        bytesRead = fread(buffer, 1, partsLen, fp);
        fwrite(buffer, 1, bytesRead, wf);
        fclose(wf);
    }

    printf("Total size : %d\n", totalSize);
    printf("Number of parts : %d\n", parts);
    printf("Length of each part : %d\n", partsLen);
    fclose(fp);
    return parts;
}

void addFileInFolder(char *destDir, uint16_t seq, char *buffer, int msgLen)
{
    char f[30];

    sprintf(f, "%s/file%d.txt", destDir, seq);
    FILE *wf = fopen(f, "wb");

    fwrite(&seq, sizeof(uint16_t), 1, wf);
    fwrite(buffer, sizeof(char), msgLen, wf);
    fclose(wf);
    // printf("File saved successfully %d\n", seq);
}

void combineCurrentFiles(char *folder, char *fileName)
{
    char outputPath[300];
    sprintf(outputPath, "%s/%s", folder, fileName);

    FILE *output = fopen(outputPath, "wb");
    if (output == NULL){
        perror("Output file not open");
        return;
    }

    uint16_t expectedSequence = 1;
    while (1)
    {
        DIR *dir = opendir(folder);
        if (dir == NULL)
        {
            printf("Cannot open directory.\n");
            fclose(output);
            return;
        }

        struct dirent *entry;
        int found = 0;

        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char path[300];
            sprintf(path, "%s/%s", folder, entry->d_name);

            if (strcmp(path, outputPath) == 0)
                continue;

            FILE *part = fopen(path, "rb");
            if (part == NULL)
                continue;

            uint16_t sequence;
            if (fread(&sequence, sizeof(uint16_t), 1, part) != 1)
            {
                fclose(part);
                continue;
            }

            if (sequence == expectedSequence)
            {
                uint8_t buffer[BUFLEN];
                int n = fread(buffer, 1, BUFLEN, part);
                if (n > 0)
                    fwrite(buffer, 1, n, output);
                // printf("Combining sequence = %u with %d bytes\n", sequence, n);
                found = 1;
                fclose(part);
                break;
            }
            fclose(part);
        }
        closedir(dir);
        if (!found)
            break;
        expectedSequence++;
    }
    fclose(output);
    printf("File combined successfully: %s\n", outputPath);
}