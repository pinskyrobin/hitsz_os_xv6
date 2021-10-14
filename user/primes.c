#include "kernel/types.h"
#include "user.h"
#define NUMBER 35
#define READ p[0]
#define WRITE p[1]
#define OK 0

void get_prime(int read_port) {
    int prime;
    // 递归出口
    if (read(read_port, &prime, sizeof(int)) == 0) {
        exit(OK);
    }
    printf("prime %d\n", prime);

    int p[2];
    pipe(p);

    int pid = fork();
    // 子进程读取父进程传递的数
    if (pid == 0) {
        close(WRITE);
        get_prime(READ);
    }
    // 父进程筛选它的父进程传递给它的数
    else {
        close(READ);
        int num;
        while (read(read_port, &num, sizeof(int)) != 0) {
           if (num % prime != 0) {
                write(WRITE, &num, sizeof(int));
            }
        }
        close(WRITE);
    }
    wait(OK);
    exit(OK);
}

int main(int argc, char const *argv[]) {
    int p[2];
    pipe(p);

    int pid = fork();
    // 先将 2 - 35 传递给子进程
    if (pid == 0) {
        close(WRITE);
        get_prime(READ);
        close(READ);
    } else {
        close(READ);
        for (int i = 2; i <= NUMBER; i++) {
            write(WRITE, &i, sizeof(int));
        }
        close(WRITE);
    }
    wait(OK);
    exit(OK);
}
