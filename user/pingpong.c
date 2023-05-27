#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

int main() {
  int p[2];
  int ret = pipe(p);
  if (ret != 0) {
    const char *err_msg = "create pipe failed\n";
    write(2, err_msg, strlen(err_msg));
    exit(-1);
  }
  char buf[2];

  int pid = fork();

  if (pid == 0) {
    // child
    read(p[0], buf, 1);
    close(p[0]);

    printf("%d: received ping\n", getpid());

    write(p[1], "c", 1);
    close(p[1]);

    exit(0);
  } else {
    // parent
    write(p[1], "p", 1);
    close(p[1]);

    wait(0);

    read(p[0], buf, 1);
    close(p[0]);

    printf("%d: received pong\n", getpid());
  }
  exit(0);
}