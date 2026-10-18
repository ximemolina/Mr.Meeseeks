#include "reqs.h"

static void descartarResto(const char *buf) {       /* si la línea era más larga que el buffer */
    if (strchr(buf, '\n') == NULL) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }
}

static void mostrarNiveles(Compartido *shm) {
    printf("   Mr.Meeseeks por nivel:");
    for (int i = 1; i < MAX_NIVELES; i++)
        if (shm->porNivel[i] > 0) printf(" N%d=%d", i, shm->porNivel[i]);
    printf("\n");
}

int box(Compartido *shm, Resultado tareas[]) {
    char request[TEXTO_MAX];
    int contTareas = 0;

    while (contTareas < MAX_TAREAS) {
        printf(ANSI_COLOR_RESET "\n Hola! ¿qué quiere que haga Mr.Meeseeks por usted? \n -> ");
        fflush(stdout);
        if (fgets(request, sizeof request, stdin) == NULL) break;       /* EOF = chao */
        descartarResto(request);
        request[strcspn(request, "\r\n")] = '\0';
        if (request[0] == '\0') continue;

        if (strcasestr(request, "chao") != NULL) break;                 /* terminar Box */

        Resultado *t = &tareas[contTareas];
        memset(t, 0, sizeof *t);
        snprintf(t->solicitud, sizeof t->solicitud, "%s", request);

        if (strcasestr(request, "calcule") != NULL)
            moduloCalculos(request, shm, t);                            /* 1 solo Mr.Meeseeks */
        else if (strcasestr(request, "ejecute") != NULL)
            moduloProgramas(request, shm, t);                           /* 1 solo Mr.Meeseeks */
        else
            moduloConsultasTextuales(request, shm, t);                  /* muchos Meeseeks */

        contTareas++;
        printf(ANSI_COLOR_RESET "\n [Box] Tarea %d: %s | %d Mr.Meeseeks | %.2f ms reales (sin esperas simuladas)\n",
               contTareas, t->exito ? "COMPLETADA" : "FALLIDA", t->cantMeeseeks, t->tiempoMs);
        mostrarNiveles(shm);
        if (shm->topeAlcanzado)
            printf("   Plan de contingencia activado: se alcanzó el tope de %d Mr.Meeseeks.\n", shm->tope);
    }
    return contTareas;
}

int main(int argc, char *argv[]) {
    static Resultado tareas[MAX_TAREAS];
    int tiempoMax = TIEMPO_CAOS_DEFECTO, tope = TOPE_DEFECTO;

    if (argc > 1) tiempoMax = atoi(argv[1]);     /* caos planetario en segundos (0 = sin límite) */
    if (argc > 2) tope = atoi(argv[2]);          /* máximo de Meeseeks por tarea */
    if (tiempoMax < 0 || tope < 1) {
        fprintf(stderr, "Uso: %s [tiempo_max_segundos (0=sin limite)] [tope_meeseeks]\n", argv[0]);
        return 1;
    }

    prctl(PR_SET_CHILD_SUBREAPER, 1);            /* los huérfanos pasan a Box, no a init */
    signal(SIGPIPE, SIG_IGN);                    /* escribir a un pipe cerrado devuelve EPIPE */
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    Compartido *shm = crearMemoriaCompartida();
    if (shm == NULL) return 1;
    shm->tiempoMax = tiempoMax;
    shm->tope = tope;

    int contTareas = box(shm, tareas);

    /* ---------------------------- informe final ---------------------------- */
    int ok = 0;
    for (int i = 0; i < contTareas; i++) ok += tareas[i].exito;

    printf("\n-------------------------------------------------------");
    printf("\n--------------------INFORME FINAL---------------------\n");
    printf("-------------------------------------------------------");
    printf("\n Tareas solicitadas: %d | completadas: %d | fallidas: %d\n", contTareas, ok, contTareas - ok);
    for (int i = 0; i < contTareas; i++) {
        Resultado *t = &tareas[i];
        const char *tipo = t->tipoTarea == TAREA_CALCULO ? "cálculo"
                         : t->tipoTarea == TAREA_EXTERNO ? "programa externo" : "consulta textual";
        printf("\n Tarea %d [%s]: \"%s\"", i + 1, tipo, t->solicitud);
        printf("\n   Tiempo de duración (real, sin esperas simuladas): %.2f ms", t->tiempoMs);
        printf("\n   Esperas simuladas (camino crítico): %.2f ms | tiempo total de pared: %.2f ms", t->esperaMs, t->totalMs);
        printf("\n   Estado: %s", t->exito ? "Completada con éxito" : (t->caos ? "Tarea fallida (caos planetario)" : "Tarea fallida"));
        printf("\n   Detalle: %s", t->texto);
        if (t->exito && t->tipoTarea == TAREA_TEXTO)
            printf("\n   Resuelta por Mr.Meeseeks (pid %d, ppid %d, N %d, i %d)", t->pid, t->ppid, t->snivel, t->sinst);
        printf("\n   Cantidad de Mr.Meeseeks necesitados: %d (niveles: %d)\n", t->cantMeeseeks, t->niveles);
    }

    destruirMemoriaCompartida(shm);
    return 0;
}
