#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/fs.h"

/**
 * 参考 ls.c 文件的实现
 * 在其源码基础上做出部分修改
 **/

// 根据文件目录，获取文件名
char* fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  for (p = path + strlen(path); p >= path && *p != '/'; p--);
  p++;

  if(strlen(p) >= DIRSIZ) {
    return p;
  }
  memmove(buf, p, strlen(p) + 1);
  return buf;
}

void find(char *path, char *file_name) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type) {
    case T_FILE:
      if (strcmp(fmtname(path), file_name) == 0) {
        printf("%s\n", path);
      }
      
      break;

    case T_DIR:
      if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf("find: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while(read(fd, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
          continue;
        }
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if(stat(buf, &st) < 0){
          printf("find: cannot stat %s\n", buf);
          continue;
        }
        find(buf, file_name);
      }
      break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  // 若用户输入的 find 没有任何参数
  if (argc < 2) {
      printf("Too few arguments!\n");
      exit(0);
  // 若用户输入的 find 只含有一个参数，则默认其从当前目录(.)查找
  } else if (argc ==  2) {
    find(".", argv[2]);
    exit(0);
  } else {
    find(argv[1], argv[2]);
    exit(0);
  }
  exit(0);
}
