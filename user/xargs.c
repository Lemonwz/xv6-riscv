#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

/**
 * 1. 先读取实际要执行的命令和其参数
 * 2. 从标准输入中读取之前命令的输出
 * 3. fork一个子进程，将读取到的输出按空格或换行符分离并放置到实际命令的参数位上
 * 4. 补充结束符，调用exec执行命令
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