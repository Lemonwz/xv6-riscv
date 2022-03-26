#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(){
    int p[2];
    if(pipe(p) < 0){
        exit(1);
    }
    if(fork()==0){
        // child process
        char buf[4];
        if(read(p[0], buf, sizeof(buf))>0){
            fprintf(1, "%d: received ping\n", getpid());
            write(p[1], "pong", 4);
        }
        close(p[0]);
        close(p[1]);
        exit(0);
    }
    else{
        // parent process
        char buf[4];
        write(p[1], "ping", 4);
        close(p[1]);
        if(read(p[0], buf, sizeof(buf))>0){
            fprintf(1, "%d: received pong\n", getpid());
        }
        close(p[0]);
        exit(0);
    }
}
