
#ifndef COMANDOS_IMPL_H
#define COMANDOS_IMPL_H

#include "definitions.h"

int ejecutarTarea(Compartido *shm, int tipoTarea, const char *texto,
                  double dificultad, double duracion, Resultado *res);

int separarArgumentos(const char *comando, char copia[],
                      size_t tamano, char *argv[], int maxArgs) {
    int cantidad = 0;

    snprintf(copia, tamano, "%s", comando);

    char *palabra = strtok(copia, " \t\r\n");

    while (palabra != NULL && cantidad < maxArgs - 1) {
        argv[cantidad++] = palabra;
        palabra = strtok(NULL, " \t\r\n");
    }

    argv[cantidad] = NULL;
    return cantidad;
}

int ejecutarPrograma(const char *comando, int *codigo) {
    char copia[TEXTO_MAX];
    char *argv[32];

    if (separarArgumentos(comando, copia, sizeof copia, argv, 32) == 0) {
        *codigo = -1;
        return -1;
    }

    fflush(stdout);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        *codigo = -1;
        return -1;
    }

    if (pid == 0) {
        signal(SIGPIPE, SIG_DFL);

        execvp(argv[0], argv);

        fprintf(stderr,
                "No pude ejecutar '%s': %s "
                "(Enviado por: %d, hijo de: %d)\n",
                argv[0], strerror(errno),
                (int)getpid(), (int)getppid());

        _exit(127);
    }

    int estado;

    while (waitpid(pid, &estado, 0) < 0 && errno == EINTR);

    if (WIFEXITED(estado))
        *codigo = WEXITSTATUS(estado);
    else if (WIFSIGNALED(estado))
        *codigo = 128 + WTERMSIG(estado);
    else
        *codigo = -1;

    return 0;
}

int moduloProgramas(char *request, Compartido *shm, Resultado *tarea) {
    const char *cmd = strcasestr(request, "ejecute");

    cmd = cmd ? cmd + strlen("ejecute") : request;

    while (isspace((unsigned char)*cmd))
        cmd++;

    return ejecutarTarea(shm, TAREA_EXTERNO, cmd, 0, 0, tarea);
}

#endif /* COMANDOS_IMPL_H */

