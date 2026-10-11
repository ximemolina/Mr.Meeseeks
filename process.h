#include "definitions.h"

int crearPipe(){
  char write_msg[BUFFER_SIZE];
  char read_msg[BUFFER_SIZE];
  int fd[2];
  
  if(pipe(fd) == -1){
    fprintf(stderr,"Pipe failed");
    return 1;
  }
  
}

void crearProceso(){
  pid_t pid;
  
  pid = fork();
  
  if(pid<0){
    fprintf(stderr,"Fork failed");
  }else if(pid == 0){
    printf(ANSI_COLOR_CYAN  "\n   Hi, I'm Mr. Meeseeks! Look at Meeeee. (%i, %i, 1, 1)\n" ,getpid(), getppid() );
  } else{
    wait(NULL);
    printf("\nListo terminado\n");
  }
  
  return;
}
