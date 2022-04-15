#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/*
  * `pingpong.c` just uses the `pipe()` system
  * call, however, the `pipe()` is full duplex,
  * not like Linux, is half duplex.
*/

char buf[1024];

int main(int argc, char* argv[]) {

  if(argc != 1) {
    fprintf(2, "Usage: ping pong\n");
    exit(1);
  }

  int fd[2];
  if(pipe(fd) < 0) {
    fprintf(2, "Pipe error, exit");
    exit(1);
  }

  int pid;
  if((pid = fork()) < 0) {
    fprintf(2, "Fork error, exit");
    exit(1);
  } else if (pid == 0) {
    if(read(fd[0], buf, 1) != 1) {
      fprintf(2, "Child reads error, exit");
      exit(1);
    }
    printf("%d: received ping\n", getpid());

    if(write(fd[1], "\n", 1) != 1) {
      fprintf(2, "Child writes error, exit");
      exit(1);
    }
  } else {
    if(write(fd[1],"\n", 1) != 1) {
      fprintf(2, "Parent writes error, exit");
      exit(1);
    }
    if(read(fd[0], buf, 1) != 1) {
      fprintf(2, "Parent reads error, exit");
      exit(1);
    }
    printf("%d: received pong\n", getpid());
  }

  exit(0);
}
