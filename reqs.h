#include "process.h"
#include <time.h>

//*****************Consultas textuales*******************************
int consultarDificultad(){
  char *respuesta = (char *) malloc(16);
  printf("\n Qué tan difícil es completar esa tarea?\n -> ");
  fgets(respuesta,16,stdin);
  
  if(strstr(respuesta, "No se")!=NULL) // dejarlo a la suerte del Mr.Meeseeks creado
  { 
    return 0;
  } else
  { 
    return strtol(respuesta,NULL,10);
  }
  
  free(respuesta);
}

int consultarDuración(){
  char *respuesta=(char *) malloc(16);
  printf("\n Qué tanto se durá completando esa tarea?\n -> ");
  fgets(respuesta,16,stdin);
  
  if(strstr(respuesta, "No se")!=NULL) // dejarlo a la suerte del Mr.Meeseeks creado
  { 
    return rand() % 310; //un poco más de 5mins para que haya posibilidad de caos planetario
  } else
  { 
    return strtol(respuesta,NULL,10);
  }
  
  free(respuesta);

}

void completarTarea(int duracion, int dificultad, int tarea[]){ // retorna 1 si exito, 0 si fallo
  clock_t comienzo, fin;
  int contProcesos = 0;
  
  comienzo = clock();
  contProcesos++;
  crearProceso();
  
  if(dificultad > 85){
    sleep(duracion);
    printf(ANSI_COLOR_CYAN"\n He completado su solicitud, un gusto haberle servido, chao!   (Enviado por:%i , hijo de:%i )\n",getpid(),getppid());
    fin = clock();
    tarea[0] = 1;
    tarea[1] = fin-comienzo;
    tarea[2] = contProcesos;
    return; 
  }
}

void moduloConsultasTextuales(char *request, int tarea[]){
  completarTarea(consultarDuración(),consultarDificultad(),tarea);
}

//****************Cálculos*******************************************
void moduloCalculos(char *request){
    if(strstr(request, "AND")!=NULL) {
        crearProceso();
        printf("\nEstá bien! Voy a hacer un AND\n");
        printf("\nlisto ya termine\n");
        return;
    } 
    else if(strstr(request, "OR")!=NULL){
      printf("\nEstá bien! Voy a hacer un OR\n");
      return;
    } else if(strstr(request, "NOT")!=NULL){
    printf("\nEstá bien! Voy a hacer un NOT\n");
    return;
    } else if(strstr(request, "XOR")!=NULL){
    printf("\nEstá bien! Voy a hacer un XOR\n");
    return;
    }
}

