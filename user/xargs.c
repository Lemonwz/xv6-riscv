#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

/**
 * 1. 从标准输入中读取参数
 * 2. 一个字符一个字符的读并存入para字符数组直到读到换行符(“\n”)
 * 3. fork一个子进程，调用exec(xargs后没有指定cmd的话默认echo），参数为para
 */
int
main(int argc, char *argv[]){
    int i, argIndex=0;
    char *cmd;
    char line[512];
    char *args[MAXARG];

    cmd = argc<2?"echo":argv[1];
    for(i=1; i<argc; ++i){
        args[argIndex++] = argv[i];
    }

    while(read(0, line, sizeof(line))){
        if(fork()==0){
            // printf("%s", line);
            int i, paraIndex=0;
            char *para = (char *) malloc(sizeof(line));
            for(i=0; i<sizeof(line); ++i){
                if(line[i] == ' ' || line[i] == '\n'){
                    // printf("%s", para);
                    para[paraIndex] = 0;
                    args[argIndex++] = para;
                    paraIndex = 0;
                    para = (char *) malloc(sizeof(line));
                }else{
                    para[paraIndex++] = line[i];
                }
            }
            para[paraIndex] = 0;
            args[argIndex] = 0;
            exec(cmd, args);
        }else{
            wait(0);
        }
    }
    exit(0);
}