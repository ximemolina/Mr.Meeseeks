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
#include <semaphore.h>

#define BUFFER_SIZE 64
#define READ_END    0
#define WRITE_END   1

#define MAX_TAREAS          1000
#define MAX_NIVELES         64
#define TEXTO_MAX           256
#define MAX_HIJOS           8      /* 5 base + hasta 3 extras */
#define TOPE_DEFECTO        60     /* máximo de Mr.Meeseeks por tarea */
#define TIEMPO_CAOS_DEFECTO 300    /* segundos (5 min) */

#define ANSI_COLOR_CYAN   "\x1b[36m"
#define ANSI_COLOR_GREEN  "\x1b[32m"
#define ANSI_COLOR_RED    "\x1b[31m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_RESET  "\x1b[0m"

/* Tipos de tarea */
enum { TAREA_TEXTO = 0, 
       TAREA_CALCULO = 1, 
       TAREA_EXTERNO = 2 
};

/* Tipos de mensaje que viajan por los pipes */
enum { MSG_SOLICITUD = 1,  /* padre -> hijo: la solicitud del usuario   */
       MSG_TERMINAR  = 2,  /* padre -> hijo: alguien resolvió, despídete */
       MSG_EXITO     = 3,  /* hijo -> padre: tarea resuelta              */
       MSG_FALLO     = 4   /* hijo -> padre: no se pudo                  */ 
};

/* Mensaje de tamaño fijo (< PIPE_BUF, así cada write() es atómico) */
typedef struct {
    int    tipo;
    int    tipoTarea;
    int    pid, ppid, snivel, sinst;   /* datos del Meeseeks que resolvió */
    double dificultad;                 /* 0..100, -1 = "a la suerte"      */
    double duracion;                   /* segundos, -1 = aleatoria 0.5-5  */
    double esperaMs;                   /* espera simulada en camino crítico */
    char   texto[TEXTO_MAX];
} Mensaje;

/* Memoria compartida entre Box y todos los Meeseeks */
typedef struct {
    sem_t mutex;                  /* semáforo binario que protege lo de abajo */
    int   total;                  /* Meeseeks creados para la tarea actual    */
    int   vivos;                  /* Meeseeks vivos ahora                     */
    int   porNivel[MAX_NIVELES];  /* Meeseeks iniciados en cada nivel N       */
    int   topeAlcanzado;          /* 1 si se activó el plan de contingencia   */
    int   tope;                   /* configuración: máximo de Meeseeks        */
    int   tiempoMax;              /* configuración: caos planetario (s), 0=sin límite */
} Compartido;

/* Resultado de una tarea, para el informe final */
typedef struct {
    char   solicitud[TEXTO_MAX];
    char   texto[TEXTO_MAX];
    int    tipoTarea;
    int    exito;          /* 1 -> éxito | 0 -> fallo */
    int    caos;           /* 1 si terminó por caos planetario */
    int    cantMeeseeks;
    int    niveles;
    int    pid, ppid, snivel, sinst;   /* quién resolvió */
    double tiempoMs;       /* tiempo real SIN las esperas simuladas */
    double esperaMs;       /* esperas simuladas (camino crítico)    */
    double totalMs;        /* tiempo de pared total                 */
} Resultado;

#endif
