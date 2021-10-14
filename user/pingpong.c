#include "kernel/types.h"
#include "user.h"
#define BUFFER_SIZE 5

int main(int argc, char* argv[]) {
    int p[2];
    int q[2];
    int pid;
    int child_status;
    char* buffer = (char *) malloc(BUFFER_SIZE * sizeof(char));
    pipe(p);
    pipe(q);
    int ret = fork();
    if (ret == 0) {
        pid = getpid();

        close(p[1]); // 关闭写端
        read(p[0], buffer, BUFFER_SIZE);
        printf("%d: received %s\n", pid, buffer);
        close(p[0]); // 读取完成，关闭读端

        close(q[0]); // 关闭读端
        write(q[1], "pong", BUFFER_SIZE);
        close(q[1]); // 写入完成，关闭写端

        exit(0);
    } else if (ret > 0) {
        pid = getpid();

        close(p[0]); // 关闭读端
        write(p[1], "ping", BUFFER_SIZE);
        close(p[1]); // 写入完成，关闭写端

        close(q[1]); // 关闭写端
        read(q[0], buffer, BUFFER_SIZE);
        printf("%d: received %s\n", pid, buffer);
        close(q[0]); // 读取完成，关闭读端

        wait(&child_status);
    }
    exit(0);
}