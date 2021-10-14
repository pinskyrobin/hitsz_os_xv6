#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#define STDIN 0

int main(int argc, char *argv[]) {
    char stdin_buf[MAXARG];     // 标准输入流的缓冲区
    char *args[MAXARG];         // 参数列表

    // 将 xargs 的参数传给将要执行的命令
    for (int i = 1; i < argc; i++) {
        args[i - 1] = argv[i];
    }

    char *start_arg = stdin_buf;    // 一个参数的首地址
    char *ch = stdin_buf;           // 指向将要接收的字符
    int args_idx = argc - 1;        // 用于获取当前参数列表的索引
    int stdin_input_size;           // 接收从输入流得到的字符数
    while ((stdin_input_size = read(STDIN, stdin_buf, sizeof(stdin_buf))) > 0) {
        for (int i = 0; i < stdin_input_size; i++) {

            // 如果命令输入完成
            if (stdin_buf[i] == '\n') {
                // 将最后一个参数结束
                *ch = 0;
                // 并传给参数列表
                args[args_idx++] = start_arg;
                // 参数列表以 0 作结
                args[args_idx] = 0;
                // 执行命令
                if (fork() == 0) {
                    exec(argv[1], args);
                } else {
                    wait(0);
                }
                // 还原各变量，准备执行下一次命令
                args_idx = argc - 1;
                ch = stdin_buf;
                start_arg = stdin_buf;

            // 如果某个参数输入完成
            } else if(stdin_buf[i] == ' ') {
                // 将该参数结束
                *ch++ = 0;
                // 并传递给参数列表
                args[args_idx++] = start_arg;
                // 将下一参数的首地址指向将要接收的字符
                start_arg = ch;

            // 接收参数尚不完整
            } else {
                // 接收该参数的下一个字符
                *ch++ = stdin_buf[i];
            }
        }
    }
    // 键入 Ctrl+d 标准输入流获取 0 个字符，程序退出
    exit(0);
}
