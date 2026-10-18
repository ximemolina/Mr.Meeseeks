#include "definitions.h"

// Semáforo
Compartido *crearMemoriaCompartida(void) {
    Compartido *shm = mmap(NULL, sizeof(Compartido), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    // pshared = 1 -> compartido entre procesos, valor inicial = 1
    if (sem_init(&shm->mutex, 1, 1) == -1) {
        perror("sem_init");
        munmap(shm, sizeof(Compartido));
        return NULL;
    }

    shm->contador = 0;
    return shm;
}

void destruirMemoriaCompartida(Compartido *shm) {
    sem_destroy(&shm->mutex);
    munmap(shm, sizeof(Compartido));
}

// Pipe
int crearPipe(int fd[2]) {
    if (pipe(fd) == -1) {
        perror("pipe");
        return -1;
    }
    return 0;
}


void trabajoMeeseeks(int id, int fd[2], Compartido *shm) {
    char msg[BUFFER_SIZE];

    close(fd[READ_END]);                 // el hijo solo escribe

    printf(ANSI_COLOR_CYAN "\n   Hi, I'm Mr. Meeseeks! Look at Meeeee. (%d, %d, %d)\n" ANSI_COLOR_RESET, getpid(), getppid(), id);

    // sección critica
    sem_wait(&shm->mutex);
    shm->contador++;
    int valor = shm->contador;
    sem_post(&shm->mutex);

    /* enviar mensaje al padre por el pipe */
    snprintf(msg, BUFFER_SIZE, "Meeseeks %d (pid %d): contador=%d",
             id, getpid(), valor);
    write(fd[WRITE_END], msg, strlen(msg) + 1);

    close(fd[WRITE_END]);
    munmap(shm, sizeof(Compartido));
    _exit(0);                            /* el hijo termina aquí */
}


pid_t crearProcesoMeeseeks(int id, int fd[2], Compartido *shm) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        trabajoMeeseeks(id, fd, shm);    /* no retorna */
    }
    return pid;                          /* padre */
}


int setupProceso(char * request) {
    const int N = 5;                     /* cantidad de Meeseeks */
    int fd[2];
    char buffer[BUFFER_SIZE];

    // Crear semáforo y pipe ANTES de los fork
    Compartido *shm = crearMemoriaCompartida();
    if (shm == NULL) return 1;

    if (crearPipe(fd) == -1) {
        destruirMemoriaCompartida(shm);
        return 1;
    }

    // Crear MrMeeseeks
    int creados = 0;
    for (int i = 1; i <= N; i++) {
        if (crearProcesoMeeseeks(i, fd, shm) > 0) creados++;
    }

    // El padre solo lee: cierra el extremo de escritura.
    close(fd[WRITE_END]);

    ssize_t n;
    while ((n = read(fd[READ_END], buffer, BUFFER_SIZE)) > 0) {
        printf("Padre recibió: %s\n", buffer);
    }
    close(fd[READ_END]);

    for (int i = 0; i < creados; i++) wait(NULL); // esperar por hijos

    printf("\nContador final: %d (esperado: %d)\n", shm->contador, creados);

    destruirMemoriaCompartida(shm);
    return 0;
}
