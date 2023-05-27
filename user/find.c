#include "common.h"

char *getBaseName(char *path) {
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  return p;
}

void find(char *path, char *target) {
  // printf("path = %s, target = %s\n", path, target);

  char buf[512];
  char *p;

  struct dirent de;
  struct stat st;

  int fd;

  if ((fd = open(path, 0)) < 0) {
    return;
  }

  char *basename = getBaseName(path);
  // printf("basename = %s\n", basename);

  if (strcmp(basename, target) == 0) {
    printf("%s\n", path);
  }

  if (fstat(fd, &st) < 0) {
    // fprintf(2, "cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type == T_DIR) {
    strcpy(buf, path);
    p = buf + strlen(path);
    int pre_len = strlen(path);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0) {
        continue;
      }
      if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
        continue;
      }
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      find(buf, target);
    }
    buf[pre_len] = '\0';
  }
  close(fd);
  return;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(2, "use find path target\n");
    exit(0);
  }

  find(argv[1], argv[2]);

  exit(0);
}