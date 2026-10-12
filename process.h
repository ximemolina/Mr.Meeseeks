#include "definitions.h"

int crearProcesoInicial(char *request){
  char write_msg[BUFFER_SIZE];
  char read_msg[BUFFER_SIZE];
  int info[2]; 

  
  pid_t pid;
  
  pid = fork();
  
  if(pid<0){
    fprintf(stderr,"Fork failed");
  }else if(pid == 0){
    printf(ANSI_COLOR_CYAN  "\n   Hi, I'm Mr. Meeseeks! Look at Meeeee. (%i, %i, 1, 1)\n" ,getpid(), getppid() );
    return 0; // 0 significa que es hijo
  } else{
    wait(NULL);
    printf("\nSoy la terminal que creó el primer mr meeseek entonces no me despido\n");
  }
  
  return 1; // 1 significa que es padre
}
