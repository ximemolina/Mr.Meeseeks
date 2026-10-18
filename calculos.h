
#ifndef CALCULOS_IMPL_H
#define CALCULOS_IMPL_H

#include "definitions.h"

int ejecutarTarea(Compartido *shm, int tipoTarea, const char *texto,
                  double dificultad, double duracion, Resultado *res);

typedef struct {
    const char *pos;
    const char *error;
} Analizador;

long long parsearOr(Analizador *a);

void saltarEspacios(Analizador *a) {
    while (isspace((unsigned char)*a->pos))
        a->pos++;
}

int aceptarSimbolo(Analizador *a, char c) {
    saltarEspacios(a);

    if (*a->pos == c) {
        a->pos++;
        return 1;
    }

    return 0;
}

int aceptarPalabra(Analizador *a, const char *palabra) {
    saltarEspacios(a);

    size_t largo = strlen(palabra);

    if (strncasecmp(a->pos, palabra, largo) == 0 &&
        !isalnum((unsigned char)a->pos[largo])) {
        a->pos += largo;
        return 1;
    }

    return 0;
}

long long parsearPrimario(Analizador *a) {
    saltarEspacios(a);

    if (a->error)
        return 0;

    if (*a->pos == '(') {
        a->pos++;
        long long valor = parsearOr(a);

        if (!a->error && !aceptarSimbolo(a, ')'))
            a->error = "falta cerrar un parentesis";

        return valor;
    }

    if (isdigit((unsigned char)*a->pos)) {
        char *fin;
        errno = 0;

        long long valor = strtoll(a->pos, &fin, 10);

        if (errno == ERANGE)
            a->error = "numero fuera de rango";

        a->pos = fin;
        return valor;
    }

    a->error = (*a->pos != '\0')
        ? "token inesperado"
        : "expresion incompleta";

    return 0;
}

long long parsearUnario(Analizador *a) {
    if (aceptarSimbolo(a, '-'))
        return -parsearUnario(a);

    if (aceptarSimbolo(a, '+'))
        return parsearUnario(a);

    if (aceptarSimbolo(a, '~') || aceptarPalabra(a, "NOT"))
        return ~parsearUnario(a);

    return parsearPrimario(a);
}

long long parsearProducto(Analizador *a) {
    long long valor = parsearUnario(a);

    while (!a->error) {
        if (aceptarSimbolo(a, '*')) {
            valor *= parsearUnario(a);
        } else if (aceptarSimbolo(a, '/')) {
            long long divisor = parsearUnario(a);

            if (a->error)
                break;

            if (divisor == 0) {
                a->error = "division por cero";
                break;
            }

            valor /= divisor;
        } else if (aceptarSimbolo(a, '%')) {
            long long divisor = parsearUnario(a);

            if (a->error)
                break;

            if (divisor == 0) {
                a->error = "modulo por cero";
                break;
            }

            valor %= divisor;
        } else {
            break;
        }
    }

    return valor;
}

long long parsearSuma(Analizador *a) {
    long long valor = parsearProducto(a);

    while (!a->error) {
        if (aceptarSimbolo(a, '+'))
            valor += parsearProducto(a);
        else if (aceptarSimbolo(a, '-'))
            valor -= parsearProducto(a);
        else
            break;
    }

    return valor;
}

long long parsearAnd(Analizador *a) {
    long long valor = parsearSuma(a);

    while (!a->error &&
           (aceptarSimbolo(a, '&') || aceptarPalabra(a, "AND")))
        valor &= parsearSuma(a);

    return valor;
}

long long parsearXor(Analizador *a) {
    long long valor = parsearAnd(a);

    while (!a->error &&
           (aceptarSimbolo(a, '^') || aceptarPalabra(a, "XOR")))
        valor ^= parsearAnd(a);

    return valor;
}

long long parsearOr(Analizador *a) {
    long long valor = parsearXor(a);

    while (!a->error &&
           (aceptarSimbolo(a, '|') || aceptarPalabra(a, "OR")))
        valor |= parsearXor(a);

    return valor;
}

int evaluarExpresion(const char *expresion,
                     long long *resultado,
                     const char **error) {
    Analizador a = { expresion, NULL };

    *resultado = parsearOr(&a);

    if (!a.error) {
        saltarEspacios(&a);

        if (*a.pos != '\0')
            a.error = "sobran caracteres en la expresion";
    }

    if (a.error) {
        *error = a.error;
        return -1;
    }

    return 0;
}

int moduloCalculos(char *request, Compartido *shm, Resultado *tarea) {
    const char *expr = strcasestr(request, "calcule");

    expr = expr ? expr + strlen("calcule") : request;

    while (isspace((unsigned char)*expr))
        expr++;

    return ejecutarTarea(shm, TAREA_CALCULO, expr, 0, 0, tarea);
}

#endif /* CALCULOS_IMPL_H */

