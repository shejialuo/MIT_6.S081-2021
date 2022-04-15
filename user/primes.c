#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NULL 0

/*
  * `pingpong.c` just uses the `pipe()` system
  * call, however, the `pipe()` is full duplex,
  * not like Linux, is half duplex.
*/

/*
  * A function to create a new process as the
  * pipeline and return the pipe to its parent
*/
int *pipeline() {

  /*
    * num is used to receive the value from the parent
  */
  int num = 0;

  int *fd = malloc(2* sizeof(int));

  if(pipe(fd) < 0) {
    fprintf(2, "Pipe error, exit\n");
    exit(1);
  }

  int pid;
  if((pid = fork()) < 0) {
    fprintf(2, "Fork error, exit\n");
    exit(1);
  } else if(pid > 0) {
    /*
      * For parent, it needs the file descriptor to writes,
      * Because for a pipeline, it could be a child, but it
      * could be also be a parent.
    */
    return fd;
  } else {
    /*
      * We need to first closed unused file descriptor
    */
    close(fd[1]);

    /*
      * When the pipeline is constructed first time,
      * it means that there is a new prime, so we need
      * to read from the parent first, and print the value
    */
    int isConstructed = 0;

    read(fd[0], &num, 4);
    int primeNumber = num;
    printf("prime %d\n", primeNumber);

    int* f = NULL;
    while(read(fd[0], &num, 4) != 0) {
      if(num % primeNumber != 0) {
        if(!isConstructed) {
          f = pipeline();
          close(f[0]);
          isConstructed = 1;
        }
        write(f[1], &num, 4);
      }
    }
    /*
      * However, for situwation where a child isn't another
      * process' parent, we should do nothing
    */
    if(isConstructed) {
      /*
        * Info the child to exit, because there are no
        * values to receive, thus that read(fd[0], &num, 4)
        * would return 0
      */
      close(f[1]);
      // free the memory
      free(f);
      wait(NULL);
    }

    exit(0);
  }
  return NULL;
}

int main(int argc, char* argv[]) {

  if(argc != 1) {
    fprintf(2, "Usage: primes\n");
    exit(1);
  }

  /*
    * We make `main` function as the header of the pipeline,
    * so here we just print the "prime 2"
  */
  printf("prime 2\n");

  int isConstructed = 0;
  int *fd = NULL;
  for(int i = 3; i < 35; ++i) {
    /*
      * If the value cannot be divided by prime value, we need to
      * construct a pipeline (`fork` a child) for the first time,
      * which means we just need a flag to indicate whether the
      * pipeline is constructed.
    */
    if(i % 2 != 0) {
      if(!isConstructed) {
        isConstructed = 1;
        fd = pipeline();
        close(fd[0]);
      }
      write(fd[1], &i, 4);
    }

  }
  /*
    * Although we know there must have piplines after `main`,
    * but we should do this properly.
  */
  if(isConstructed) {
    /*
      * Fire the gun to info its child that there are no values,
      * thus to exit the program sequence by sequence
    */
    close(fd[1]);
    free(fd);
    wait(NULL);
  }

  exit(0);
}
