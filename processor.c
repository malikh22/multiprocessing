#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/wait.h>
#include "png.h"

// Struct to hold the file name and histogram data
typedef struct {
    char filename[256];
    int histogram[256*3];
} Payload;

// Struct to hold the thread arguments
typedef struct {
  char *filename;
  int sharedMemoryPosition;
} ThreadArgs;


// Global variables
sem_t *semaphore;
Payload *shared_memory;
char * output_file;
static char files[35000][256];


// Function to open directory and iterate over image files
int getFiles(char *folder) {
    DIR *dir;
    struct dirent *ent;
    int num_files = 0;

    if ((dir = opendir(folder)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_REG) {
                sprintf((char *)files[num_files], "%s/%s", folder, ent->d_name);
                num_files++;
            }
        }
        closedir(dir);
    } else {
        perror("Could not open directory");
        return -1;
    }

    return num_files;
}

//function to create a shared memory segment
int createSharedMemory(int n) {
    int shared_mem_fd = shm_open("payload", O_CREAT | O_RDWR, 0666);
    ftruncate(shared_mem_fd, n * sizeof(Payload));
    shared_memory = (Payload *) mmap(NULL, n * sizeof(Payload), PROT_READ | PROT_WRITE, MAP_SHARED, shared_mem_fd, 0);

    return shared_mem_fd;
}

// Function to extract color histogram from an image file
void *getHistogram(char *filename) {
    Payload *payload = (Payload*)malloc(sizeof(Payload));

    //start of Dr. Cenek's code -- do not modify
    char* fn_in = filename;
    unsigned char* img;
    png_structp png_ptr; // pointer to png struct
    png_infop info_ptr;  //poiner to png header struct
    png_uint_32 width, height, bit_depth, color_type, interlace_type, number_of_passes;
    png_bytep * row_pointers; // pointer to image payload
    int hist[256*3] = { 0 };  // histogram array
    char header[8];           // to read magic number of 8 bytes

    /* open file and test for it being a png */
    FILE *fp = fopen(fn_in, "rb");
    if (!fp)
        perror("fopen");
    fread(header, 1, 8, fp); // read magic number
    if (png_sig_cmp(header, 0, 8))
        perror("not a PNG file");

    /* initialize png structs */
    if((png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))==0)
        perror("png_create_read_struct failed");

    if((info_ptr = png_create_info_struct(png_ptr))==0)
        perror("png_create_info_struct failed");

    if (setjmp(png_jmpbuf(png_ptr)))
        perror("init_io failed");

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    //load structs
    png_read_info(png_ptr, info_ptr);
    //get local variable copies from structs
    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    //for inflating
    number_of_passes = png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    // read file
    if (setjmp(png_jmpbuf(png_ptr)))
        perror("png_jmpbuf: error read_image");
    // allocated array of row pointers
    row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
    // allocated each row to read data into
    for (int y=0; y<height; y++)
            row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr,info_ptr));
    // read image into the 2D array
    png_read_image(png_ptr, row_pointers);
    // done reading so close file
    fclose(fp);
    //calculate color histogram counts as long as the file format has [][][] of 0-255
    if (png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_RGB)
        perror("must be a RGB file");
    for (int y=0; y<height; y++) {
        png_byte* row = row_pointers[y];
        for (int x=0; x<width; x++) {
            png_byte* ptr = &(row[x*3]);

            hist[(int)ptr[0]]++;
            hist[(int)ptr[1]+256]++;
            hist[(int)ptr[2]+256+256]++;
        }
    }
    //store histogram data in payload (this is the only part you need to modify if at all)
    for (int i = 0; i < 256; i++) {
        payload->histogram[i] = hist[i];
        payload->histogram[i+256] = hist[i+256];
        payload->histogram[i+256+256] = hist[i+256+256];
    }

    //store filename in payload (added by me)
    strcpy(payload->filename, filename);

    //memory cleanup
    for (int y=0; y<height; y++)
        free(row_pointers[y]);
    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    //end of Dr. Cenek's code
    
    return (void *) payload;
}

// Write payload to shared memory and unlock the semaphore
void * writeSharedMemory(void * args) {
    ThreadArgs * threadArgs = (ThreadArgs *) args;
    char * filename = threadArgs->filename;
    int i = threadArgs->sharedMemoryPosition;
    Payload* payload = (Payload*) getHistogram(filename);
    memcpy(&shared_memory[i], payload, sizeof(Payload));
    free(payload);
    free(args);
    sem_post(semaphore);
    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    int n = atoi(argv[1]);
    char s = argv[2][0];
    char *folder = argv[3];
    output_file = argv[4];
    int numFiles = getFiles(folder);

    // Get all files in the folder
    if (numFiles == -1) {
     exit(1);
    } else if (numFiles == 0) {
        printf("No files found in folder %s - exiting program now ... ", folder);
        exit(1);
    } else if (numFiles > 35000) {
        printf("Too many files in folder %s - exiting program now ... ", folder);
        exit(1);
    }

    // semaphore initialize to n
    semaphore = sem_open("/my_semaphore", O_CREAT, 0644, n); 
    if (semaphore == SEM_FAILED) {
        perror("sem_open");
        return 1;
    }


    // Create shared memory
    int shared_mem_fd = createSharedMemory(numFiles);

    pthread_t *threads = malloc(sizeof(pthread_t) * numFiles);
    pid_t *pids = malloc(sizeof(pid_t) * numFiles);

    // Create threads or processes and pass file names to them
    for (int i = 0; i < numFiles; i++) {
        // Create struct to pass to thread/process
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->filename = files[i];
        args->sharedMemoryPosition = i;

        sem_wait(semaphore);
        // Create thread/process and pass payload struct as argument
        if (s == 't') {
            pthread_t thread;
            pthread_attr_t attr;
            int codeRetour = pthread_attr_init(&attr);
            if (codeRetour != 0) {
                perror("pthread_attr_init");
                exit(1);
            }
            codeRetour = pthread_create(&thread, &attr, &writeSharedMemory, (void *) args);
            if (codeRetour != 0) {
                perror("pthread_create");
                exit(1);
            }
            threads[i] = thread;

        } else if (s == 'p') {
            // Create process and attach shared memory to it
            int pid = fork();
            if (pid == 0) {
                Payload * payload = (Payload *)getHistogram(files[i]);
                memcpy(&shared_memory[i], payload, sizeof(Payload));
                free(payload);
                free(args);
                free(pids);
                free(threads);
                sem_post(semaphore);
                exit(0);
            }
            free(args);
            pids[i] = pid;

        } else {
            perror("Invalid argument for s");
            exit(1);
        }

    }

     //destroy created threads, if any 
    if (s == 't') {
        
        for (int i = 0; i < numFiles; i++) {
            pthread_join(threads[i], NULL);
        }
    } else if (s == 'p') {
        // Wait for all processes to finish
        for (int i = 0; i < numFiles; i++) {
            waitpid(pids[i], NULL, 0);
        }
    }

    // Merge histogram data from shared memory
    int hist[256 * 3] = {0};
    for (int i = 0; i < numFiles; i++) {
        for (int j = 0; j < 256 * 3; j++) {
            hist[j] += shared_memory[i].histogram[j];
        }
    }

    // Write histogram data to output file
    FILE *fp = fopen(output_file, "w");
    for (int i = 0; i < 256; i++) {
        for(int j = 0; j < 3; j++) {
            fprintf(fp, "%d ", hist[i*3+j]);
        }  
        fprintf(fp, "\n");
    }

    fclose(fp);

    // Clean up resources
    sem_close(semaphore);
    sem_unlink("/my_semaphore");
    munmap(shared_memory, numFiles * sizeof(Payload));
    shm_unlink("payload");
    free(threads);
    free(pids);

    return 0;
}

