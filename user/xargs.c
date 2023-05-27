#include "common.h"

#define MAXBUFSIZE

void copy(char **dest, char *target) {
  *dest = malloc(strlen(target));
  strcpy(*dest, target);
}

int getline(char **paras, int cur_len) {
  int bufsize = 1024;
  char buf[bufsize];
  int read_idx = 0;
  while (read(0, buf + read_idx, 1) == 1) {
    // 在 \n 处截断
    if (buf[read_idx] == '\n') {
      buf[read_idx] = 0;
      break;
    }
    read_idx++;
    if (read_idx == bufsize) {
      fprintf(2, "too long parameter\n");
      exit(-1);
    }
  }

  // 没有读取内容
  if (read_idx == 0) {
    return -1;
  }

  int put_idx = 0;

  // 将buf复制到paras
  while (put_idx < read_idx) {
    if (cur_len > MAXARG) {
      fprintf(2, "too many args\n");
      exit(-1);
    }
    // 去除buf首尾的空白部分
    int buf_start = put_idx;
    while (buf_start < read_idx && buf[buf_start] == ' ') {
      buf_start++;
    }
    int buf_end = buf_start;
    while (buf_end < read_idx && buf[buf_end] != ' ') {
      buf_end++;
    }
    buf[buf_end++] = '\0';
    put_idx = buf_end;
    copy(&paras[cur_len++], &buf[buf_start]);
  }
  return cur_len;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Please enter more parameters!\n");
    exit(1);
  }

  char *paras[MAXARG];
  // 将参数集体前移一位
  for (int i = 1; i < argc; i++) {
    copy(&paras[i - 1], argv[i]);
  }

  int end_idx;
  while ((end_idx = getline(paras, argc - 1)) != -1) {
    paras[end_idx] = 0;
    if (fork() == 0) {
      exec(paras[0], paras);
      exit(-1);
    } else {
      wait(0);
    }
  }
  exit(0);
}