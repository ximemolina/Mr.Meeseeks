#include "definitions.h"



/* =========================================================================
 *  UTILIDADES
 * ========================================================================= */

/* Tiempo actual en milisegundos (reloj monotónico: no cambia si ajustan la hora del sistema) */
double ahoraMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

/* Número aleatorio en [0, 1) */
double aleatorio01(void) {
    return rand() / (RAND_MAX + 1.0);
}

/* Después de un fork() todos los procesos tienen la misma semilla.
   Hay que cambiarla para que cada Mr.Meeseeks genere números distintos. */
void reiniciarSemilla(int sal) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    srand((unsigned)(ts.tv_nsec ^ (ts.tv_sec << 7)
                     ^ ((unsigned)getpid() * 2654435761u)
                     ^ ((unsigned)sal * 40503u)));
}

/* Mensaje de un Mr.Meeseeks para el usuario. Siempre lleva su pid y ppid. */
void decir(const char *formato, ...) {
    char texto[512];
    va_list args;

    va_start(args, formato);
    vsnprintf(texto, sizeof texto, formato, args);
    va_end(args);

    printf(ANSI_COLOR_CYAN "%s (Enviado por: %d, hijo de: %d)" ANSI_COLOR_RESET "\n",
           texto, (int)getpid(), (int)getppid());
    fflush(stdout);
}


/* =========================================================================
 *  MEMORIA COMPARTIDA Y SEMÁFORO
 * ========================================================================= */

/* Memoria compartida entre Box y todos los Mr.Meeseeks.
   Todo lo que se modifique ahí dentro debe hacerse con el semáforo tomado. */

/* ------------------------- Crear y destruir ------------------------- */

Compartido *crearMemoriaCompartida(void) {
    Compartido *shm = mmap(NULL, sizeof(Compartido), PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    memset(shm, 0, sizeof(Compartido));

    /* sem_init(sem, 1, 1): el primer 1 = compartido entre procesos, el segundo = valor inicial */
    if (sem_init(&shm->mutex, 1, 1) == -1) {
        perror("sem_init");
        munmap(shm, sizeof(Compartido));
        return NULL;
    }

    shm->tope = TOPE_DEFECTO;
    shm->tiempoMax = TIEMPO_CAOS_DEFECTO;
    return shm;
}

void destruirMemoriaCompartida(Compartido *shm) {
    sem_destroy(&shm->mutex);
    munmap(shm, sizeof(Compartido));
}

/* Se llama al iniciar cada tarea. Se reinicia también el semáforo, por si un
   proceso murió por el caos planetario justo mientras lo tenía tomado. */
void reiniciarContadores(Compartido *shm) {
    sem_destroy(&shm->mutex);
    sem_init(&shm->mutex, 1, 1);
    shm->total = 1;                 /* el Mr.Meeseeks inicial ya cuenta */
    shm->vivos = 0;
    shm->topeAlcanzado = 0;
    memset(shm->porNivel, 0, sizeof shm->porNivel);
}

/* ------------------------------ Semáforo ------------------------------ */

void entrarSeccionCritica(Compartido *shm) {
    while (sem_wait(&shm->mutex) == -1 && errno == EINTR);
}

void salirSeccionCritica(Compartido *shm) {
    sem_post(&shm->mutex);
}

/* ---------------- Operaciones sobre los contadores compartidos ---------------- */

void registrarMeeseeks(Compartido *shm, int nivel) {
    entrarSeccionCritica(shm);
    shm->vivos++;
    if (nivel < MAX_NIVELES) shm->porNivel[nivel]++;
    salirSeccionCritica(shm);
}

void registrarSalida(Compartido *shm) {
    entrarSeccionCritica(shm);
    shm->vivos--;
    salirSeccionCritica(shm);
}

/* Plan de contingencia: un Mr.Meeseeks que quiere crear 'pedidos' ayudantes
   reserva el cupo aquí. Si se pasaría del tope, solo se le concede lo que quede
   (puede ser 0). Así una tarea imposible nunca crea procesos sin límite. */
int reservarCupo(Compartido *shm, int pedidos) {
    entrarSeccionCritica(shm);

    int disponibles = shm->tope - shm->total;
    if (disponibles < 0) disponibles = 0;

    int concedidos = pedidos;
    if (pedidos > disponibles) {
        concedidos = disponibles;
        shm->topeAlcanzado = 1;
    }
    shm->total += concedidos;

    salirSeccionCritica(shm);
    return concedidos;
}

/* Si no se lograron crear todos los ayudantes reservados, se devuelve el cupo */
void devolverCupo(Compartido *shm, int cantidad) {
    entrarSeccionCritica(shm);
    shm->total -= cantidad;
    salirSeccionCritica(shm);
}


/* =========================================================================
 *  PIPES Y MENSAJES
 * ========================================================================= */

/* Pipes entre Mr.Meeseeks. Cada mensaje es un 'Mensaje' de tamaño fijo, menor que
   PIPE_BUF, así que cada write() es atómico y cada read() trae un mensaje completo. */

int crearPipe(int fd[2]) {
    if (pipe(fd) == -1) {
        perror("pipe");
        return -1;
    }
    return 0;
}

/* Devuelve 0 si se envió, -1 si falló (por ejemplo, el otro extremo ya se cerró) */
int enviar(int fd, const Mensaje *mensaje) {
    ssize_t escritos = write(fd, mensaje, sizeof *mensaje);
    return (escritos == (ssize_t)sizeof *mensaje) ? 0 : -1;
}

/* Devuelve 1 si leyó un mensaje, 0 si el otro extremo se cerró (EOF), -1 si hubo error */
int recibir(int fd, Mensaje *mensaje) {
    ssize_t leidos;
    do {
        leidos = read(fd, mensaje, sizeof *mensaje);
    } while (leidos < 0 && errno == EINTR);

    if (leidos == (ssize_t)sizeof *mensaje) return 1;
    return (leidos == 0) ? 0 : -1;
}


/* ===========================================================================
 *  Módulo B: evaluador de expresiones aritméticas y lógicas
 *
 *  Operadores, de menor a mayor precedencia:
 *      OR  |        (lógico bit a bit)
 *      XOR ^
 *      AND &
 *      +  -
 *      *  /  %
 *      unarios:  -  +  ~  NOT
 *      paréntesis y números enteros
 *
 *  Es un analizador recursivo descendente: hay una función por nivel de
 *  precedencia, y cada una llama a la del nivel siguiente (más fuerte).
 * =========================================================================== */

typedef struct {
    const char *pos;     /* por dónde va leyendo la expresión */
    const char *error;   /* NULL si todo va bien; si no, descripción del error */
} Analizador;

long long parsearOr(Analizador *a);   /* declarada antes porque los paréntesis la usan */

/* ---------------------------- Lectura de texto ---------------------------- */

void saltarEspacios(Analizador *a) {
    while (isspace((unsigned char)*a->pos)) a->pos++;
}

/* Si el siguiente símbolo es 'c', lo consume y devuelve 1 */
int aceptarSimbolo(Analizador *a, char c) {
    saltarEspacios(a);
    if (*a->pos == c) {
        a->pos++;
        return 1;
    }
    return 0;
}

/* Si la siguiente palabra es 'palabra' (AND, OR, ...), la consume y devuelve 1.
   Se revisa que no siga una letra, para no confundir OR con ORANGE. */
int aceptarPalabra(Analizador *a, const char *palabra) {
    saltarEspacios(a);
    size_t largo = strlen(palabra);
    if (strncasecmp(a->pos, palabra, largo) == 0 && !isalnum((unsigned char)a->pos[largo])) {
        a->pos += largo;
        return 1;
    }
    return 0;
}

/* ----------------- Un nivel de precedencia por función ----------------- */

/* Número o expresión entre paréntesis */
long long parsearPrimario(Analizador *a) {
    saltarEspacios(a);
    if (a->error) return 0;

    if (*a->pos == '(') {
        a->pos++;
        long long valor = parsearOr(a);
        if (!a->error && !aceptarSimbolo(a, ')')) a->error = "falta cerrar un parentesis";
        return valor;
    }

    if (isdigit((unsigned char)*a->pos)) {
        char *fin;
        errno = 0;
        long long valor = strtoll(a->pos, &fin, 10);
        if (errno == ERANGE) a->error = "numero fuera de rango";
        a->pos = fin;
        return valor;
    }

    a->error = (*a->pos != '\0') ? "token inesperado" : "expresion incompleta";
    return 0;
}

/* Operadores de un solo operando:  -x   +x   ~x   NOT x */
long long parsearUnario(Analizador *a) {
    if (aceptarSimbolo(a, '-')) return -parsearUnario(a);
    if (aceptarSimbolo(a, '+')) return parsearUnario(a);
    if (aceptarSimbolo(a, '~') || aceptarPalabra(a, "NOT")) return ~parsearUnario(a);
    return parsearPrimario(a);
}

/* *  /  % */
long long parsearProducto(Analizador *a) {
    long long valor = parsearUnario(a);

    while (!a->error) {
        if (aceptarSimbolo(a, '*')) {
            valor *= parsearUnario(a);
        } else if (aceptarSimbolo(a, '/')) {
            long long divisor = parsearUnario(a);
            if (a->error) break;
            if (divisor == 0) { a->error = "division por cero"; break; }
            valor /= divisor;
        } else if (aceptarSimbolo(a, '%')) {
            long long divisor = parsearUnario(a);
            if (a->error) break;
            if (divisor == 0) { a->error = "modulo por cero"; break; }
            valor %= divisor;
        } else {
            break;
        }
    }
    return valor;
}

/* +  - */
long long parsearSuma(Analizador *a) {
    long long valor = parsearProducto(a);

    while (!a->error) {
        if (aceptarSimbolo(a, '+'))      valor += parsearProducto(a);
        else if (aceptarSimbolo(a, '-')) valor -= parsearProducto(a);
        else break;
    }
    return valor;
}

/* AND  & */
long long parsearAnd(Analizador *a) {
    long long valor = parsearSuma(a);

    while (!a->error && (aceptarSimbolo(a, '&') || aceptarPalabra(a, "AND")))
        valor &= parsearSuma(a);
    return valor;
}

/* XOR  ^ */
long long parsearXor(Analizador *a) {
    long long valor = parsearAnd(a);

    while (!a->error && (aceptarSimbolo(a, '^') || aceptarPalabra(a, "XOR")))
        valor ^= parsearAnd(a);
    return valor;
}

/* OR  |   (el nivel más bajo: aquí empieza todo) */
long long parsearOr(Analizador *a) {
    long long valor = parsearXor(a);

    while (!a->error && (aceptarSimbolo(a, '|') || aceptarPalabra(a, "OR")))
        valor |= parsearXor(a);
    return valor;
}

/* ------------------------------ Punto de entrada ------------------------------ */

/* Evalúa 'expresion'. Devuelve 0 y deja el valor en *resultado, o devuelve -1 y
   deja en *error el motivo (por ejemplo "division por cero"). */
int evaluarExpresion(const char *expresion, long long *resultado, const char **error) {
    Analizador a = { expresion, NULL };

    *resultado = parsearOr(&a);

    if (!a.error) {                 /* no debe sobrar texto después de la expresión */
        saltarEspacios(&a);
        if (*a.pos != '\0') a.error = "sobran caracteres en la expresion";
    }
    if (a.error) {
        *error = a.error;
        return -1;
    }
    return 0;
}


/* ===========================================================================
 *  Módulo C: ejecutar un programa externo y esperar su código de retorno.
 *  Código 0 = el programa terminó bien; cualquier otro valor = falló.
 * =========================================================================== */

/* Parte "ls -l /tmp" en {"ls", "-l", "/tmp", NULL}. Devuelve la cantidad de palabras. */
int separarArgumentos(const char *comando, char copia[], size_t tamano, char *argv[], int maxArgs) {
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

/* Ejecuta el comando en un proceso nuevo y espera a que termine.
   Devuelve 0 si pudo ejecutarlo (el código de retorno queda en *codigo),
   o -1 si no pudo ni crear el proceso. */
int ejecutarPrograma(const char *comando, int *codigo) {
    char copia[TEXTO_MAX];
    char *argv[32];

    if (separarArgumentos(comando, copia, sizeof copia, argv, 32) == 0) {
        *codigo = -1;
        return -1;
    }

    fflush(stdout);                     /* evita que el hijo repita texto pendiente */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        *codigo = -1;
        return -1;
    }

    if (pid == 0) {                     /* proceso hijo: se convierte en el programa externo */
        signal(SIGPIPE, SIG_DFL);       /* el programa externo no debe heredar el SIG_IGN */
        execvp(argv[0], argv);
        fprintf(stderr, "No pude ejecutar '%s': %s (Enviado por: %d, hijo de: %d)\n",
                argv[0], strerror(errno), (int)getpid(), (int)getppid());
        _exit(127);                     /* solo se llega aquí si execvp falló */
    }

    int estado;
    while (waitpid(pid, &estado, 0) < 0 && errno == EINTR);
    *codigo = WIFEXITED(estado) ? WEXITSTATUS(estado) : 128 + WTERMSIG(estado);
    return 0;
}


/* ===========================================================================
 *  Mr. Meeseeks. Todos los procesos Meeseeks ejecutan este mismo código.
 *
 *  Ciclo de vida de uno:
 *     1. Saluda y se registra en la memoria compartida.
 *     2. Recibe la solicitud por su pipe privado.
 *     3. Según el tipo de tarea:
 *          - cálculo / programa externo: lo hace él solo.
 *          - consulta textual: espera un tiempo y, según la dificultad, o la
 *            resuelve, o crea ayudantes (hijos) con una dificultad menor.
 *     4. Responde a su padre por el pipe, se despide y desaparece.
 *
 *  Cada Meeseeks tiene dos pipes con su padre:
 *     down_r : de aquí lee las órdenes del padre (solicitud, "terminen")
 *     up_w   : aquí escribe su respuesta al padre (éxito / fallo)
 * =========================================================================== */

/* Todo lo que un Meeseeks necesita saber de sí mismo */
typedef struct {
    Compartido *shm;
    int down_r;
    int up_w;
    int nivel;      /* N */
    int inst;       /* i */
} Contexto;

/* Un ayudante (hijo) y los dos pipes para hablar con él */
typedef struct {
    pid_t pid;
    int   down_w;   /* le escribo órdenes */
    int   up_r;     /* leo su respuesta */
    int   activo;   /* 1 mientras no haya respondido */
} Hijo;

typedef struct {
    Hijo lista[MAX_HIJOS];
    int  cantidad;
} ListaHijos;

void meeseeks(Compartido *shm, int down_r, int up_w, int nivel, int inst);

/* ---------------------------------------------------------------------------
 *  Manejo de los hijos
 * --------------------------------------------------------------------------- */

void cerrarPipesDeHijos(ListaHijos *hijos) {
    for (int k = 0; k < hijos->cantidad; k++) {
        if (hijos->lista[k].down_w >= 0) { close(hijos->lista[k].down_w); hijos->lista[k].down_w = -1; }
        if (hijos->lista[k].up_r   >= 0) { close(hijos->lista[k].up_r);   hijos->lista[k].up_r   = -1; }
    }
}

/* Les ordena a los hijos que terminen (todos, menos 'excepto'; usar -1 para todos) */
void ordenarTerminarAHijos(ListaHijos *hijos, int excepto) {
    Mensaje orden;
    memset(&orden, 0, sizeof orden);
    orden.tipo = MSG_TERMINAR;

    for (int k = 0; k < hijos->cantidad; k++)
        if (k != excepto && hijos->lista[k].down_w >= 0)
            enviar(hijos->lista[k].down_w, &orden);   /* si el hijo ya salió, falla sin problema */
}

/* Espera a que todos los hijos desaparezcan (cada uno se despide antes de irse) */
void esperarQueTerminenHijos(ListaHijos *hijos) {
    for (int k = 0; k < hijos->cantidad; k++) {
        int estado;
        while (waitpid(hijos->lista[k].pid, &estado, 0) < 0 && errno == EINTR);
    }
    cerrarPipesDeHijos(hijos);
}

/* ---------------------------------------------------------------------------
 *  Despedirse y terminar
 * --------------------------------------------------------------------------- */

void despedirse(Contexto *ctx) {
    decir("Chao! Fue un gusto haberle servido.");
    registrarSalida(ctx->shm);
}

/* Termina el proceso sin avisarle nada al padre (el padre mismo pidió terminar) */
void salirSinResponder(Contexto *ctx) {
    despedirse(ctx);
    close(ctx->up_w);
    close(ctx->down_r);
    _exit(0);
}

/* Termina el proceso enviándole 'respuesta' al padre.
   El Meeseeks original (nivel 1) además informa al usuario el resultado final.
   ESTA FUNCIÓN NO VUELVE: el proceso desaparece aquí. */
void responderYSalir(Contexto *ctx, Mensaje *respuesta) {
    if (ctx->nivel == 1) {
        if (respuesta->tipo == MSG_EXITO)
            decir("Solicitud completada. La resolvio Mr. Meeseeks (pid %d, ppid %d, N %d, i %d). %s",
                  respuesta->pid, respuesta->ppid, respuesta->snivel, respuesta->sinst, respuesta->texto);
        else
            decir("No pude cumplir la solicitud: %s", respuesta->texto);
    }
    despedirse(ctx);
    enviar(ctx->up_w, respuesta);
    close(ctx->up_w);
    close(ctx->down_r);
    _exit(0);
}

/* ---------------------------------------------------------------------------
 *  Tarea B: cálculo, y tarea C: programa externo (un solo Meeseeks)
 * --------------------------------------------------------------------------- */

void atenderCalculo(Contexto *ctx, Mensaje *solicitud, Mensaje *respuesta) {
    long long resultado;
    const char *error = "";

    decir("Calculando: %s", solicitud->texto);

    if (evaluarExpresion(solicitud->texto, &resultado, &error) == 0) {
        respuesta->tipo = MSG_EXITO;
        snprintf(respuesta->texto, sizeof respuesta->texto, "Resultado: %lld", resultado);
    } else {
        respuesta->tipo = MSG_FALLO;
        snprintf(respuesta->texto, sizeof respuesta->texto, "Error de calculo: %s", error);
    }
    responderYSalir(ctx, respuesta);
}

void atenderPrograma(Contexto *ctx, Mensaje *solicitud, Mensaje *respuesta) {
    int codigo;

    decir("Ejecutando programa: %s", solicitud->texto);

    if (ejecutarPrograma(solicitud->texto, &codigo) == 0 && codigo == 0) {
        respuesta->tipo = MSG_EXITO;
        snprintf(respuesta->texto, sizeof respuesta->texto, "El programa termino con codigo 0");
    } else {
        respuesta->tipo = MSG_FALLO;
        snprintf(respuesta->texto, sizeof respuesta->texto, "El programa fallo (codigo %d)", codigo);
    }
    responderYSalir(ctx, respuesta);
}

/* ---------------------------------------------------------------------------
 *  Tarea A: consulta textual (puede crear una jerarquía de Meeseeks)
 * --------------------------------------------------------------------------- */

/* Dificultad al azar (caso 2 del enunciado): normal(media 50, desviación 25)
   truncada a [0, 100]. Se aproxima sumando 12 uniformes (Irwin-Hall), así no
   se necesita la biblioteca matemática. Un valor truncado a 0 es una tarea imposible. */
double muestrearDificultad(void) {
    double suma = 0;
    for (int i = 0; i < 12; i++) suma += aleatorio01();

    double dificultad = 50.0 + 25.0 * (suma - 6.0);
    if (dificultad < 0)   dificultad = 0;
    if (dificultad > 100) dificultad = 100;
    return dificultad;
}

/* Espera 'ms' milisegundos simulando que trabaja, pero escuchando a su padre.
   Devuelve 1 si el padre ordenó terminar (otro Meeseeks ya resolvió la tarea).
   En *real deja cuánto esperó de verdad: ese tiempo no cuenta como tiempo real. */
int esperarSimulado(int down_r, double ms, double *real) {
    double inicio = ahoraMs();
    double fin = inicio + ms;
    Mensaje mensaje;

    for (;;) {
        double resto = fin - ahoraMs();
        if (resto <= 0) break;

        struct pollfd escucha = { down_r, POLLIN, 0 };
        int listos = poll(&escucha, 1, (int)(resto + 0.999));

        if (listos < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (listos > 0) {
            int leido = recibir(down_r, &mensaje);
            if (leido <= 0 || mensaje.tipo == MSG_TERMINAR) {
                *real = ahoraMs() - inicio;
                return 1;
            }
        }
    }
    *real = ahoraMs() - inicio;
    return 0;
}

/* Cuántos ayudantes pide según la dificultad:
     0 a 45      -> al menos 5
     45.01 a 85  -> al menos 2
   Cuanto más difícil, más probable que pida hasta 3 ayudantes extra. */
int calcularCantidadAyudantes(double dificultad) {
    int cantidad = (dificultad <= 45.0) ? 5 : 2;

    for (int extra = 0; extra < 3; extra++)
        if (aleatorio01() < (100.0 - dificultad) / 300.0)
            cantidad++;
    return cantidad;
}

/* Dilución ("aging"): la dificultad que recibe cada hijo es más cercana a 100,
   y mejora más cuantos más ayudantes se crearon.
   Si la dificultad es 0 (imposible) nunca mejora. */
double dificultadParaHijo(double dificultadPadre, int cantidadHijos) {
    if (dificultadPadre <= 0) return 0.0;

    double factor = 0.30 + 0.06 * cantidadHijos;
    double variacion = 0.85 + 0.30 * aleatorio01();      /* cada hijo recibe algo distinto */
    double dificultad = dificultadPadre + (100.0 - dificultadPadre) * factor * variacion;

    return (dificultad > 100.0) ? 100.0 : dificultad;
}

/* Crea un hijo (instancia 'numero') y le envía su solicitud por un pipe privado.
   Devuelve 0 si lo creó, -1 si no se pudo. */
int crearHijo(Contexto *ctx, ListaHijos *hijos, int numero, Mensaje *solicitudHijo) {
    int bajada[2];   /* padre -> hijo */
    int subida[2];   /* hijo -> padre */

    if (crearPipe(bajada) == -1) return -1;
    if (crearPipe(subida) == -1) {
        close(bajada[0]); close(bajada[1]);
        return -1;
    }

    fflush(stdout);
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        close(bajada[0]); close(bajada[1]);
        close(subida[0]); close(subida[1]);
        return -1;
    }

    if (pid == 0) {
        /* Proceso hijo: cierra todo lo que no es suyo y se convierte en un Meeseeks */
        close(bajada[1]);
        close(subida[0]);
        cerrarPipesDeHijos(hijos);          /* los pipes de sus hermanos mayores */
        close(ctx->down_r);                 /* los pipes que su padre usa con el abuelo */
        close(ctx->up_w);
        meeseeks(ctx->shm, bajada[0], subida[1], ctx->nivel + 1, numero);
        _exit(0);
    }

    /* Proceso padre: guarda los extremos que le tocan y le manda la solicitud */
    close(bajada[0]);
    close(subida[1]);

    Hijo *nuevo = &hijos->lista[hijos->cantidad++];
    nuevo->pid = pid;
    nuevo->down_w = bajada[1];
    nuevo->up_r = subida[0];
    nuevo->activo = 1;

    enviar(bajada[1], solicitudHijo);
    return 0;
}

/* Crea hasta 'cantidad' hijos con la misma solicitud pero dificultad menor */
void crearHijos(Contexto *ctx, ListaHijos *hijos, Mensaje *solicitud, double dificultad, int cantidad) {
    for (int numero = 1; numero <= cantidad; numero++) {
        Mensaje solicitudHijo;
        memset(&solicitudHijo, 0, sizeof solicitudHijo);
        solicitudHijo.tipo = MSG_SOLICITUD;
        solicitudHijo.tipoTarea = TAREA_TEXTO;
        solicitudHijo.duracion = solicitud->duracion;
        solicitudHijo.dificultad = dificultadParaHijo(dificultad, cantidad);
        snprintf(solicitudHijo.texto, sizeof solicitudHijo.texto, "%s", solicitud->texto);

        if (crearHijo(ctx, hijos, numero, &solicitudHijo) == -1) break;
    }
}

/* Mientras espera a sus hijos, también escucha a su padre. Si el padre ordena
   terminar, este Meeseeks manda terminar a sus hijos, se despide y desaparece. */
void atenderOrdenDelPadre(Contexto *ctx, ListaHijos *hijos) {
    Mensaje orden;
    int leido = recibir(ctx->down_r, &orden);

    if (leido <= 0 || orden.tipo == MSG_TERMINAR) {
        ordenarTerminarAHijos(hijos, -1);
        esperarQueTerminenHijos(hijos);
        salirSinResponder(ctx);
    }
}

/* Espera las respuestas de los hijos hasta que uno resuelva o todos fallen.
   Devuelve el índice del hijo que resolvió (con su mensaje en *respuestaGanador),
   o -1 si todos fallaron (con la mayor espera simulada vista en *mayorEspera). */
int esperarRespuestasDeHijos(Contexto *ctx, ListaHijos *hijos,
                             Mensaje *respuestaGanador, double *mayorEspera) {
    int pendientes = hijos->cantidad;
    *mayorEspera = 0;

    while (pendientes > 0) {
        /* Se vigila el pipe del padre (posición 0) y el de cada hijo que no ha respondido */
        struct pollfd escucha[MAX_HIJOS + 1];
        int indiceHijo[MAX_HIJOS + 1];
        int total = 0;

        escucha[total].fd = ctx->down_r;
        escucha[total].events = POLLIN;
        escucha[total].revents = 0;
        indiceHijo[total++] = -1;

        for (int k = 0; k < hijos->cantidad; k++) {
            if (!hijos->lista[k].activo) continue;
            escucha[total].fd = hijos->lista[k].up_r;
            escucha[total].events = POLLIN;
            escucha[total].revents = 0;
            indiceHijo[total++] = k;
        }

        if (poll(escucha, total, -1) < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (escucha[0].revents)
            atenderOrdenDelPadre(ctx, hijos);

        for (int i = 1; i < total; i++) {
            if (!escucha[i].revents) continue;

            int k = indiceHijo[i];
            Mensaje respuesta;
            int leido = recibir(hijos->lista[k].up_r, &respuesta);

            hijos->lista[k].activo = 0;
            pendientes--;

            if (leido > 0 && respuesta.tipo == MSG_EXITO) {
                *respuestaGanador = respuesta;
                return k;
            }
            if (leido > 0 && respuesta.esperaMs > *mayorEspera)
                *mayorEspera = respuesta.esperaMs;
        }
    }
    return -1;
}

/* La dificultad es <= 85: este Meeseeks no puede solo, así que pide ayuda. */
void pedirAyuda(Contexto *ctx, Mensaje *solicitud, Mensaje *respuesta, double dificultad) {
    double miEspera = respuesta->esperaMs;

    /* 1. Cuántos ayudantes, respetando el tope (plan de contingencia) */
    int cantidad = calcularCantidadAyudantes(dificultad);
    cantidad = reservarCupo(ctx->shm, cantidad);

    if (cantidad == 0) {
        respuesta->tipo = MSG_FALLO;
        snprintf(respuesta->texto, sizeof respuesta->texto,
                 "limite de %d Mr.Meeseeks alcanzado, no puedo pedir mas ayuda", ctx->shm->tope);
        decir("Ya hay demasiados Meeseeks (tope %d). No puedo pedir ayuda.", ctx->shm->tope);
        responderYSalir(ctx, respuesta);
    }

    decir("Esto es dificil (%.1f). Pido ayuda a %d Mr. Meeseeks (nivel %d).",
          dificultad, cantidad, ctx->nivel + 1);

    /* 2. Crear los ayudantes */
    ListaHijos hijos;
    hijos.cantidad = 0;
    crearHijos(ctx, &hijos, solicitud, dificultad, cantidad);

    if (hijos.cantidad < cantidad)
        devolverCupo(ctx->shm, cantidad - hijos.cantidad);

    if (hijos.cantidad == 0) {
        respuesta->tipo = MSG_FALLO;
        snprintf(respuesta->texto, sizeof respuesta->texto, "no pude crear ayudantes");
        responderYSalir(ctx, respuesta);
    }

    /* 3. Esperar a que uno resuelva o todos fallen */
    Mensaje respuestaGanador;
    double mayorEspera;
    int ganador = esperarRespuestasDeHijos(ctx, &hijos, &respuestaGanador, &mayorEspera);

    /* 4. Responderle a mi padre */
    if (ganador >= 0) {
        decir("Mi ayudante (pid %d) resolvio la tarea. Aviso a los demas y me voy.",
              (int)hijos.lista[ganador].pid);
        ordenarTerminarAHijos(&hijos, ganador);     /* los hermanos se despiden */
        esperarQueTerminenHijos(&hijos);

        *respuesta = respuestaGanador;              /* conserva los datos de quien resolvió */
        respuesta->tipo = MSG_EXITO;
        respuesta->esperaMs = miEspera + respuestaGanador.esperaMs;
    } else {
        esperarQueTerminenHijos(&hijos);

        respuesta->tipo = MSG_FALLO;
        respuesta->esperaMs = miEspera + mayorEspera;
        snprintf(respuesta->texto, sizeof respuesta->texto,
                 "ni mis ayudantes ni yo pudimos con la tarea");
    }
    responderYSalir(ctx, respuesta);
}

void atenderTextual(Contexto *ctx, Mensaje *solicitud, Mensaje *respuesta) {
    /* Dificultad: la dio el usuario, o se deja a la suerte (valor negativo) */
    double dificultad = solicitud->dificultad;
    if (dificultad < 0) {
        dificultad = muestrearDificultad();
        decir("Lo dejaron a mi suerte: estimo la dificultad en %.1f", dificultad);
    }

    /* Duración simulada: la dio el usuario, o aleatoria entre 0.5 y 5 s */
    double duracion = (solicitud->duracion >= 0) ? solicitud->duracion
                                                 : 0.5 + 4.5 * aleatorio01();
    decir("Dificultad %.1f. Voy a intentarlo durante %.2f s...", dificultad, duracion);

    double esperaReal;
    if (esperarSimulado(ctx->down_r, duracion * 1000.0, &esperaReal))
        salirSinResponder(ctx);             /* otro Meeseeks ya resolvió la tarea */
    respuesta->esperaMs = esperaReal;

    if (dificultad > 85.0) {                /* 85.01 a 100: lo resuelve él solo */
        respuesta->tipo = MSG_EXITO;
        snprintf(respuesta->texto, sizeof respuesta->texto, "Tarea '%.200s' lista.", solicitud->texto);
        decir("He completado su solicitud, un gusto haberle servido!");
        responderYSalir(ctx, respuesta);
    }

    pedirAyuda(ctx, solicitud, respuesta, dificultad);
}

/* ---------------------------------------------------------------------------
 *  Punto de entrada de todo Mr.Meeseeks
 * --------------------------------------------------------------------------- */

void meeseeks(Compartido *shm, int down_r, int up_w, int nivel, int inst) {
    Contexto ctx = { shm, down_r, up_w, nivel, inst };
    Mensaje solicitud, respuesta;

    reiniciarSemilla(nivel * 100 + inst);

    printf(ANSI_COLOR_CYAN "\nHi, I'm Mr. Meeseeks! Look at Meeeee. (%d, %d, %d, %d)" ANSI_COLOR_RESET "\n",
           (int)getpid(), (int)getppid(), nivel, inst);
    fflush(stdout);
    registrarMeeseeks(shm, nivel);

    /* La solicitud llega por mi pipe privado */
    if (recibir(down_r, &solicitud) <= 0 || solicitud.tipo != MSG_SOLICITUD) {
        despedirse(&ctx);
        _exit(1);
    }

    /* La respuesta ya lleva mis datos, por si soy yo quien resuelve */
    memset(&respuesta, 0, sizeof respuesta);
    respuesta.pid = getpid();
    respuesta.ppid = getppid();
    respuesta.snivel = nivel;
    respuesta.sinst = inst;

    switch (solicitud.tipoTarea) {
        case TAREA_CALCULO: atenderCalculo(&ctx, &solicitud, &respuesta); break;
        case TAREA_EXTERNO: atenderPrograma(&ctx, &solicitud, &respuesta); break;
        default:            atenderTextual(&ctx, &solicitud, &respuesta);  break;
    }
    _exit(0);   /* nunca se llega aquí: todas las ramas terminan el proceso */
}


/* ===========================================================================
 *  Lado de Box: crea el Mr.Meeseeks inicial, le pasa la solicitud por un pipe,
 *  espera su respuesta y vigila el tiempo máximo (caos planetario).
 * =========================================================================== */

typedef struct {
    pid_t pid;
    int   down_w;   /* Box -> Meeseeks inicial */
    int   up_r;     /* Meeseeks inicial -> Box */
} MeeseeksInicial;

/* Crea el Meeseeks inicial (nivel 1, instancia 1). Devuelve 0 si pudo, -1 si no. */
int crearMeeseeksInicial(Compartido *shm, MeeseeksInicial *inicial) {
    int bajada[2], subida[2];

    if (crearPipe(bajada) == -1) return -1;
    if (crearPipe(subida) == -1) {
        close(bajada[0]); close(bajada[1]);
        return -1;
    }

    fflush(stdout);
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        close(bajada[0]); close(bajada[1]);
        close(subida[0]); close(subida[1]);
        return -1;
    }

    if (pid == 0) {
        setpgid(0, 0);                  /* grupo propio: así Box puede matarlos a todos juntos */
        close(bajada[1]);
        close(subida[0]);
        meeseeks(shm, bajada[0], subida[1], 1, 1);
        _exit(0);
    }

    setpgid(pid, pid);
    close(bajada[0]);
    close(subida[1]);
    inicial->pid = pid;
    inicial->down_w = bajada[1];        /* se mantiene abierto hasta que termine la tarea */
    inicial->up_r = subida[0];
    return 0;
}

void enviarSolicitudInicial(MeeseeksInicial *inicial, int tipoTarea, const char *texto,
                            double dificultad, double duracion) {
    Mensaje solicitud;
    memset(&solicitud, 0, sizeof solicitud);
    solicitud.tipo = MSG_SOLICITUD;
    solicitud.tipoTarea = tipoTarea;
    solicitud.dificultad = dificultad;
    solicitud.duracion = duracion;
    snprintf(solicitud.texto, sizeof solicitud.texto, "%s", texto);

    enviar(inicial->down_w, &solicitud);
}

/* Espera la respuesta del Meeseeks inicial, pero no más allá del tiempo máximo.
   Devuelve 1 si hubo respuesta (en *respuesta), 0 si no la hubo, y pone
   *caos = 1 si se acabó el tiempo (caos planetario). */
int esperarRespuestaOCaos(Compartido *shm, MeeseeksInicial *inicial, double horaInicio,
                          Mensaje *respuesta, int *caos) {
    *caos = 0;

    for (;;) {
        int timeoutMs = -1;                         /* -1 = esperar sin límite */

        if (shm->tiempoMax > 0) {
            double resto = horaInicio + shm->tiempoMax * 1000.0 - ahoraMs();
            if (resto <= 0) { *caos = 1; return 0; }
            timeoutMs = (int)(resto + 0.999);
        }

        struct pollfd escucha = { inicial->up_r, POLLIN, 0 };
        int listos = poll(&escucha, 1, timeoutMs);

        if (listos < 0) {
            if (errno == EINTR) continue;
            return 0;
        }

        return recibir(inicial->up_r, respuesta) > 0;
    }
}

/* Caos planetario: se termina a todos los Meeseeks de la tarea de una vez */
void declararCaosPlanetario(Compartido *shm, MeeseeksInicial *inicial) {
    printf(ANSI_COLOR_RED "\n CAOS PLANETARIO: pasaron %d s sin resolver la tarea. "
           "Terminando todos los Mr.Meeseeks.\n" ANSI_COLOR_RESET, shm->tiempoMax);
    fflush(stdout);
    kill(-inicial->pid, SIGKILL);                   /* el signo menos = todo el grupo de procesos */
}

/* Espera que desaparezca el Meeseeks inicial y, si hubo caos, también a todos los
   demás (Box es "subreaper": los nietos huérfanos pasan a ser hijos suyos). */
void recogerProcesos(MeeseeksInicial *inicial, int hubocaos) {
    int estado;
    while (waitpid(inicial->pid, &estado, 0) < 0 && errno == EINTR);

    if (hubocaos)
        while (waitpid(-1, NULL, 0) > 0 || errno == EINTR);

    close(inicial->down_w);
    close(inicial->up_r);
}

/* Guarda en 'res' las estadísticas de la tarea */
void llenarResultado(Compartido *shm, Resultado *res, double totalMs,
                     int recibido, Mensaje *respuesta, int caos) {
    res->totalMs = totalMs;
    res->cantMeeseeks = shm->total;

    res->niveles = 0;
    for (int i = 1; i < MAX_NIVELES; i++)
        if (shm->porNivel[i] > 0) res->niveles = i;

    if (caos) {
        res->caos = 1;
        res->esperaMs = 0;
        res->tiempoMs = totalMs;
        snprintf(res->texto, sizeof res->texto, "caos planetario (tiempo maximo excedido)");
        return;
    }

    if (recibido) {
        res->exito = (respuesta->tipo == MSG_EXITO);
        res->esperaMs = respuesta->esperaMs;
        res->pid = respuesta->pid;
        res->ppid = respuesta->ppid;
        res->snivel = respuesta->snivel;
        res->sinst = respuesta->sinst;
        snprintf(res->texto, sizeof res->texto, "%s", respuesta->texto);
    }

    /* Tiempo real = tiempo total - esperas simuladas */
    res->tiempoMs = totalMs - res->esperaMs;
    if (res->tiempoMs < 0) res->tiempoMs = 0;
}

/* Ejecuta una tarea completa. Devuelve 1 si se completó, 0 si no. */
int ejecutarTarea(Compartido *shm, int tipoTarea, const char *texto,
                  double dificultad, double duracion, Resultado *res) {
    MeeseeksInicial inicial;
    Mensaje respuesta;
    int caos = 0;

    res->tipoTarea = tipoTarea;
    res->exito = 0;
    res->caos = 0;
    snprintf(res->texto, sizeof res->texto, "no se pudo iniciar la tarea");

    reiniciarContadores(shm);

    double horaInicio = ahoraMs();
    if (crearMeeseeksInicial(shm, &inicial) == -1) return 0;

    enviarSolicitudInicial(&inicial, tipoTarea, texto, dificultad, duracion);

    int recibido = esperarRespuestaOCaos(shm, &inicial, horaInicio, &respuesta, &caos);
    if (caos) declararCaosPlanetario(shm, &inicial);
    recogerProcesos(&inicial, caos);

    llenarResultado(shm, res, ahoraMs() - horaInicio, recibido, &respuesta, caos);
    return res->exito;
}
