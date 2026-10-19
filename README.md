# Tarea 3 de Sistemas Operativos: Mr. Meeseeks Box

Programa en C para Linux que simula una "caja" de Mr. Meeseeks. El usuario escribe una solicitud y Box crea (con `fork`) uno o varios procesos Mr. Meeseeks que la resuelven y desaparecen. Los Meeseeks se comunican con pipes y comparten sus contadores con un semáforo POSIX.

## Qué puede hacer

| Solicitud | Qué hace |
|---|---|
| Texto libre (`quiero un café francés`) | Simula la tarea según una dificultad de 0 a 100. Si es difícil, el Meeseeks pide ayuda a otros Meeseeks |
| `calcule 2+3*4` | Resuelve una operación aritmética o lógica (`+ - * / %`, `AND OR XOR NOT`, paréntesis) |
| `ejecute ls -l` | Ejecuta un programa externo y reporta si terminó bien (código 0) o no |

Al escribir `chao`, Box muestra un informe con las tareas pedidas, su duración, cuáles se completaron y cuántos Mr. Meeseeks se usaron en cada una.

## Instalación

Requiere Linux y `gcc`.

```bash
sudo apt install build-essential     # solo si no tiene gcc
gcc -Wall -Wextra -pthread box.c -o box
```

Todos los archivos (`box.c` y los `.h`) deben estar en la misma carpeta.

## Ejecución

```bash
./box
```

Opcionalmente se pueden indicar dos valores:

```bash
./box [tiempo_max_segundos] [tope_meeseeks]
```

| Opción | Qué es | Por defecto |
|---|---|---|
| `tiempo_max_segundos` | Tiempo máximo por tarea antes del "caos planetario", que termina todos los procesos (0 = sin límite) | 300 |
| `tope_meeseeks` | Máximo de Meeseeks que se pueden crear en una tarea | 60 |

Ejemplo de uso:

```
$ ./box

 Hola! ¿Qué quiere que haga Mr.Meeseeks por usted?
 -> quiero un café francés

 ¿Qué tan difícil es completar esa tarea? (0 a 100, o 'No sé')
 -> 40

 ¿Cuánto durará la simulación? (segundos, o 'No sé')
 -> 1
```
