#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "png.h"

// Struct to hold the file name and histogram data
typedef struct {
    char filename[256];
    int histogram[256];
} Payload;

// Struct to hold the thread arguments
typedef struct {
    char *filename;
    int sharedMemoryPosition;
} ThreadArgs;

// Function prototypes
int getFiles(char *folder, char **files);
int createSharedMemory(int n);
void *getHistogram(char *filename);
void writeSharedMemory(char *filename, int i);

#endif /* __PROCESSOR_H__ */
