#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>

#define BUFFER_SIZE 64
#define READ_END    0
#define WRITE_END   1

#define ANSI_COLOR_CYAN  "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

typedef struct {
    sem_t mutex;     // semáforo binario
    int   contador;  /* recurso compartido protegido por el semáforo */
} Compartido;

#endif
