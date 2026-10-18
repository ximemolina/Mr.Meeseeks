#include "process.h"

/* ************************* Consultas textuales ************************* */

/* Devuelve 0..100, o -1 si el usuario dice "No se" (se deja a la suerte del Mr.Meeseeks) */
double consultarDificultad(void) {
    char linea[64];
    for (;;) {
        printf("\n Qué tan difícil es completar esa tarea? (0 a 100, o 'No se')\n -> ");
        fflush(stdout);
        if (fgets(linea, sizeof linea, stdin) == NULL) return -1;
        if (strcasestr(linea, "no se") != NULL) return -1;
        char *fin;
        double v = strtod(linea, &fin);
        if (fin != linea && v >= 0 && v <= 100) return v;
        printf(" Valor inválido, escriba un número entre 0 y 100.\n");
    }
}

/* Devuelve segundos (>= 0), o -1 si el usuario dice "No se" (aleatorio entre 0.5 y 5 s) */
double consultarDuracion(void) {
    char linea[64];
    for (;;) {
        printf("\n Cuánto durará completando esa tarea? (segundos, o 'No se')\n -> ");
        fflush(stdout);
        if (fgets(linea, sizeof linea, stdin) == NULL) return -1;
        if (strcasestr(linea, "no se") != NULL) return -1;
        char *fin;
        double v = strtod(linea, &fin);
        if (fin != linea && v >= 0) return v;
        printf(" Valor inválido, escriba un número de segundos (>= 0).\n");
    }
}

int moduloConsultasTextuales(char *request, Compartido *shm, Resultado *tarea) {
    double dificultad = consultarDificultad();
    double duracion   = consultarDuracion();   /* llamadas separadas: el orden queda garantizado */
    return ejecutarTarea(shm, TAREA_TEXTO, request, dificultad, duracion, tarea);
}

/* ******************************* Cálculos ******************************* */
int moduloCalculos(char *request, Compartido *shm, Resultado *tarea) {
    const char *expr = strcasestr(request, "calcule");
    expr = expr ? expr + strlen("calcule") : request;
    while (isspace((unsigned char)*expr)) expr++;
    return ejecutarTarea(shm, TAREA_CALCULO, expr, 0, 0, tarea);
}

/* ************************ Programas externos ************************ */
int moduloProgramas(char *request, Compartido *shm, Resultado *tarea) {
    const char *cmd = strcasestr(request, "ejecute");
    cmd = cmd ? cmd + strlen("ejecute") : request;
    while (isspace((unsigned char)*cmd)) cmd++;
    return ejecutarTarea(shm, TAREA_EXTERNO, cmd, 0, 0, tarea);
}
