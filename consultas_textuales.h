
#ifndef CONSULTAS_TEXTUALES_IMPL_H
#define CONSULTAS_TEXTUALES_IMPL_H

#include "definitions.h"

int ejecutarTarea(Compartido *shm, int tipoTarea, const char *texto,
                  double dificultad, double duracion, Resultado *res);

double consultarDificultad(void) {
    char linea[64];

    while (1) {
        printf("\n ¿Qué tan difícil es completar esa tarea? "
               "(0 a 100, o 'No sé')\n -> ");
        fflush(stdout);

        if (fgets(linea, sizeof linea, stdin) == NULL)
            return -1;

        if (strcasestr(linea, "no se") != NULL ||
            strcasestr(linea, "no sé") != NULL)
            return -1;

        char *fin;
        double v = strtod(linea, &fin);

        if (fin != linea && v >= 0 && v <= 100)
            return v;

        printf(" Valor inválido. Escriba un número entre 0 y 100.\n");
    }
}

double consultarDuracion(void) {
    char linea[64];

    while (1) {
        printf("\n ¿Cuánto durará la simulación? "
               "(segundos, o 'No sé')\n -> ");
        fflush(stdout);

        if (fgets(linea, sizeof linea, stdin) == NULL)
            return -1;

        if (strcasestr(linea, "no se") != NULL ||
            strcasestr(linea, "no sé") != NULL)
            return -1;

        char *fin;
        double v = strtod(linea, &fin);

        if (fin != linea && v >= 0)
            return v;

        printf(" Valor inválido. Escriba un número de segundos >= 0.\n");
    }
}

int moduloConsultasTextuales(char *request,
                             Compartido *shm,
                             Resultado *tarea) {
    double dificultad = consultarDificultad();
    double duracion = consultarDuracion();

    return ejecutarTarea(shm, TAREA_TEXTO, request,
                         dificultad, duracion, tarea);
}

#endif /* CONSULTAS_TEXTUALES_IMPL_H */

