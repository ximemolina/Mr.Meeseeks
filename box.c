#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "reqs.h"

int box(int contTareas,int estadoTarea[],int  tiempo[],int cantMeeseeks[]){
  char *request = (char *) malloc(1000);
  int tareaActual[16];
  
  while(1){
    
    printf(ANSI_COLOR_RESET"\n Hola! ¿qué quiere que haga Mr.Meeseeks por usted?\n -> ");
    fgets(request,1000,stdin);
    
    if(strstr(request, "chao")!=NULL){ // detener ejecución programa
      free(request);
      return contTareas;
    }
    
    if(strstr(request, "calcule")!=NULL) // módulo de cálculos, solo 1 mr.meeseek
    {
      moduloCalculos(request);
    } else {
      moduloConsultasTextuales(request,tareaActual);
    }
    
    estadoTarea[contTareas] = tareaActual[0];
    tiempo[contTareas] = tareaActual[1];
    cantMeeseeks[contTareas] = tareaActual[2];
    contTareas++;
  };
  
}

int main(){ 
  int contTareas = 0;
  int estadoTarea[1000]; // 1 -> exito | 0 -> fallo
  int tiempo[1000];
  int cantMeeseeks[1000];
  
  contTareas = box(contTareas, estadoTarea, tiempo, cantMeeseeks);
  
  // informe final
  printf("\n--------------------INFORME FINAL---------------------\n");
  printf("-------------------------------------------------------");
  printf("\n Cantidad de tareas realizadas: %d", contTareas);
  for (int i=0; i<=contTareas-1;i++)
  {
    printf("\n Tiempo de duración de tarea %d: %d segundos", i,tiempo[i]);
    
    if (estadoTarea[i]==1){
      printf("\n Estado de tarea: Completada con éxito");
    }else{
      printf("\n Estado de tarea: Tarea fallida");
    }
    
    printf("Cantidad de Mr.Meeseeks necesitados: %d", cantMeeseeks[i]);
  }
  
  return 1;
}
