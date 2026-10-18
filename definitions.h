
#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/epoll.h>
#include <semaphore.h>

#define BUFFER_SIZE 64
#define READ_END 0
#define WRITE_END 1

#define MAX_TAREAS 1000
#define MAX_NIVELES 64
#define TEXTO_MAX 256
#define MAX_HIJOS 8

#define TOPE_DEFECTO 60
#define TIEMPO_CAOS_DEFECTO 300

#define ANSI_COLOR_CYAN   "\x1b[36m"
#define ANSI_COLOR_GREEN  "\x1b[32m"
#define ANSI_COLOR_RED    "\x1b[31m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_RESET  "\x1b[0m"

enum {
    TAREA_TEXTO = 0,
    TAREA_CALCULO = 1,
    TAREA_EXTERNO = 2
};

enum {
    MSG_SOLICITUD = 1,
    MSG_TERMINAR = 2,
    MSG_EXITO = 3,
    MSG_FALLO = 4
};

typedef struct {
    int tipo;
    int tipoTarea;
    int pid, ppid, snivel, sinst;
    double dificultad;
    double duracion;
    double esperaMs;
    char texto[TEXTO_MAX];
} Mensaje;

typedef struct {
    sem_t mutex;
    int total;
    int vivos;
    int porNivel[MAX_NIVELES];
    int topeAlcanzado;
    int tope;
    int tiempoMax;
} Compartido;

typedef struct {
    char solicitud[TEXTO_MAX];
    char texto[TEXTO_MAX];
    int tipoTarea;
    int exito;
    int caos;
    int cantMeeseeks;
    int niveles;
    int pid, ppid, snivel, sinst;
    double tiempoMs;
    double esperaMs;
    double totalMs;
} Resultado;

#endif

