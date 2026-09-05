#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "fileFunction.h"

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

    uint16_t extraForFirst = strlen(fileName) + 3; // 3 byte -> 1 for fileSize and 2 for total seq number

    fseek(fp, 0, SEEK_END);
    int totalSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    int parts = totalSize / (mouthCap - 3); // 1 byte for msgType, 2 byte for sequence number
    if ((totalSize + extraForFirst) % (mouthCap - 3) > 0)
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
        if (i == 1)
        {
            uint8_t fileSize = strlen(fileName);
            fwrite(&fileSize, sizeof(uint8_t), 1, wf);
            fwrite(fileName, sizeof(char), fileSize, wf);
            fwrite(&parts, sizeof(uint16_t), 1, wf);
        }
        size_t bytesRead;
        if (i == 1)
            bytesRead = fread(buffer, 1, partsLen - extraForFirst, fp);
        else
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
    printf("File saved successfully %d\n", seq);
}

void combineFile(char *folder)
{
    struct dirent *entry;
    
    int count = 0;
    FILE *fp;
    int i = 1;
    do
    {
        char path[300];
        
        DIR *dir = opendir(folder);
        if (dir == NULL)
        {
            printf("Cannot open directory.\n");
            return;
        }

        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            sprintf(path, "%s/%s", folder, entry->d_name);
            // printf("%s\n", path);
            FILE *of = fopen(path, "rb");
            if (of == NULL){
                perror("not open file");
                continue;
            }
            
            uint16_t seq;
            fread(&seq, sizeof(uint16_t), 1, of);
            if (seq == i)
            {
                char buffer[6000];

                if (seq == 1)
                {
                    uint8_t fileNameSize;
                    char fileName[30];
                    fread(&fileNameSize, sizeof(uint8_t), 1, of);
                    fread(fileName, sizeof(char), fileNameSize, of);
                    fileName[fileNameSize] = '\0';
                    fread(&count, sizeof(uint16_t), 1, of);

                    char path[300];
                    sprintf(path, "%s/%s", folder, fileName);
                    fp = fopen(path, "wb+");
                    if (fp == NULL){
                        perror("File not open : ");
                        exit(0);
                    }
                }
                int n = fread(buffer, 1, 6000, of);
                fwrite(buffer, 1, n, fp);
                fclose(of);
                i++;
                break;
            }
            fclose(of);
        }
        if (i == 1)
            break;
        closedir(dir);
    } while (i <= count);

    if (i < count){
        printf("All files not found\n");
        exit(0);
    }
    fclose(fp);
}