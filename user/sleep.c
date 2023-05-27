#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    const char* err_msg = "use: sleep x\n";
    write(2, err_msg, strlen(err_msg));
    exit(-1);
  }
  int s_time = atoi(argv[1]);
  sleep(s_time);
  exit(0);
}