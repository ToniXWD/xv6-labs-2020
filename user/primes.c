#include "common.h"

void create_child(int *fd) {
  // 先关闭写端
  close(fd[1]);
  int p, q;
  // 读取本进程判断的质因数，存入p
  read(fd[0], &p, sizeof(int));

  printf("prime %d\n", p);

  if (read(fd[0], &q, sizeof(int))) {
    if (q % p != 0) {
      int fd1[2];
      if (pipe(fd1) != 0) {
        fprintf(2, "create pipe failed\n");
        exit(-1);
      }
      if (fork() == 0) {
        create_child(fd1);
      } else {
        // 父进程关闭读端
        close(fd1[0]);
        do {
          if (q % p != 0) {
            write(fd1[1], &q, sizeof(int));
          }
        } while (read(fd[0], &q, sizeof(int)));
        close(fd1[1]);
        close(fd[0]);
        wait(0);
      }
    }
  }
  exit(0);
}

int main() {
  int fd0[2];
  if (pipe(fd0) != 0) {
    fprintf(2, "create pipe failed\n");
    exit(-1);
  }

  if (fork() == 0) {
    // child
    create_child(fd0);
  } else {
    // parent
    // 关闭管道读端
    close(fd0[0]);
    for (int i = 2; i <= 35; i++) {
      write(fd0[1], &i, sizeof(int));
    }
    close(fd0[1]);
    wait(0);
  }
  exit(0);
}