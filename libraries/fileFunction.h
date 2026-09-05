#include<stdio.h>
#ifndef FILEFUNCTION_H
#define FILEFINCTION_H

int splitParts(char *inputFile, int parts, char *dir);

void addFileInFolder(char *destDir, uint16_t seq, char *buffer, int msgLen);

void combineFile(char *dest);

#endif