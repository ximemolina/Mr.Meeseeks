
#include "definitions.h"
#include "mr_meeseeks.h"
#include "calculos.h"
#include "comandos.h"
#include "consultas_textuales.h"

void descartarResto(const char *buf) {
    if (strchr(buf, '\n') == NULL) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {}
    }
}

void mostrarNiveles(Compartido *shm) {
    printf("   Mr.Meeseeks por nivel:");
    for (int i = 1; i < MAX_NIVELES; i++)
        if (shm->porNivel[i] > 0)
            printf(" N%d=%d", i, shm->porNivel[i]);
    printf("\n");
}

int box(Compartido *shm, Resultado tareas[]) {
    char request[TEXTO_MAX];
    int contTareas = 0;

    while (contTareas < MAX_TAREAS) {
        printf(ANSI_COLOR_RESET
               "\n Hola! ¿Qué quiere que haga Mr.Meeseeks por usted?\n -> ");
        fflush(stdout);

        if (fgets(request, sizeof request, stdin) == NULL)
            break;

        descartarResto(request);
        request[strcspn(request, "\r\n")] = '\0';

        if (request[0] == '\0')
            continue;

        if (strcasestr(request, "chao") != NULL)
            break;

        Resultado *t = &tareas[contTareas];
        memset(t, 0, sizeof *t);
        snprintf(t->solicitud, sizeof t->solicitud, "%s", request);

        if (strcasestr(request, "calcule") != NULL)
            moduloCalculos(request, shm, t);
        else if (strcasestr(request, "ejecute") != NULL)
            moduloProgramas(request, shm, t);
        else
            moduloConsultasTextuales(request, shm, t);

        contTareas++;

        printf(ANSI_COLOR_RESET
               "\n Tarea %d: %s | %d Mr.Meeseeks | %.2f ms reales "
               "(sin esperas simuladas)\n",
               contTareas,
               t->exito ? "COMPLETADA" : "FALLIDA",
               t->cantMeeseeks,
               t->tiempoMs);

        mostrarNiveles(shm);

        if (shm->topeAlcanzado)
            printf(" Plan de contingencia activado: se alcanzó el tope "
                   "de %d Mr.Meeseeks.\n", shm->tope);
    }

    return contTareas;
}

int main(int argc, char *argv[]) {
    static Resultado tareas[MAX_TAREAS];
    int tiempoMax = TIEMPO_CAOS_DEFECTO;
    int tope = TOPE_DEFECTO;

    if (argc > 1)
        tiempoMax = atoi(argv[1]);

    if (argc > 2)
        tope = atoi(argv[2]);

    if (tiempoMax < 0 || tope < 1) {
        fprintf(stderr,
                " Uso: %s [tiempo_max_segundos (0=sin limite)] "
                "[tope_meeseeks]\n",
                argv[0]);
        return 1;
    }

    prctl(PR_SET_CHILD_SUBREAPER, 1);
    signal(SIGPIPE, SIG_IGN);
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    Compartido *shm = crearMemoriaCompartida();

    if (shm == NULL)
        return 1;

    shm->tiempoMax = tiempoMax;
    shm->tope = tope;

    int contTareas = box(shm, tareas);
    int ok = 0;

    for (int i = 0; i < contTareas; i++)
        ok += tareas[i].exito;

    printf("\n-------------------------------------------------------");
    printf("\n-------------------- INFORME FINAL -------------------\n");
    printf("-------------------------------------------------------");
    printf("\n Tareas solicitadas: %d | completadas: %d | fallidas: %d\n",
           contTareas, ok, contTareas - ok);

    for (int i = 0; i < contTareas; i++) {
        Resultado *t = &tareas[i];

        const char *tipo =
            t->tipoTarea == TAREA_CALCULO ? "cálculo" :
            t->tipoTarea == TAREA_EXTERNO ? "programa externo" :
            "consulta textual";

        printf("\n Tarea %d [%s]: \"%s\"",
               i + 1, tipo, t->solicitud);

        printf("\n  Tiempo real sin esperas simuladas: %.2f ms",
               t->tiempoMs);

        printf("\n  Esperas simuladas (camino crítico): %.2f ms | "
               "tiempo de pared: %.2f ms",
               t->esperaMs, t->totalMs);

        printf("\n  Estado: %s",
               t->exito ? "Completada con éxito" :
               (t->caos ? "Fallida (caos planetario)" : "Fallida"));

        printf("\n  Detalle: %s", t->texto);

        if (t->exito && t->tipoTarea == TAREA_TEXTO) {
            printf("\n  Resuelta por Mr. Meeseeks "
                   "(pid %d, ppid %d, N %d, i %d)",
                   t->pid, t->ppid, t->snivel, t->sinst);
        }

        printf("\n  Mr. Meeseeks creados: %d | niveles: %d\n",
               t->cantMeeseeks, t->niveles);
    }

    destruirMemoriaCompartida(shm);
    return 0;
}

