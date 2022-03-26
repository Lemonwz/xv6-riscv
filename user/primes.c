#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// linear version
void
primes(int p1[2], int p2[2]){
    if(fork()==0){
        int p;
        int buf[1];
        if(!read(p1[0], buf, sizeof(buf))){
            close(p1[0]);
            close(p2[0]);
            close(p2[1]);
            exit(0);
        }
        p = buf[0];
        printf("prime %d\n", p);
        while(read(p1[0], buf, sizeof(buf))){
            if(buf[0]%p != 0){
                write(p2[1], buf, sizeof(buf));
            }
        }
        close(p1[0]);
        close(p2[1]);
        if(pipe(p1) < 0){
            exit(1);
        }
        primes(p2, p1);
        exit(0);
    }else{
        close(p1[0]);
        close(p2[0]);
        close(p2[1]);
        wait(0);
        exit(0);
    }
}

// concurrent version
void
primes1(int p1[2]){
    int p;
    int p2[2];
    int buf[1];
    close(p1[1]);
    if(!read(p1[0], buf, sizeof(buf))){
        close(p1[0]);
        exit(0);
    }
    p = buf[0];
    printf("prime %d\n", p);
    if(pipe(p2) < 0){
        exit(1);
    }
    if(fork()==0){
        close(p1[0]);
        primes1(p2);
    }else{
        close(p2[0]);
        while(read(p1[0], buf, sizeof(buf))){
            if(buf[0]%p != 0){
                write(p2[1], buf, sizeof(buf));
            }
        }
        close(p1[0]);
        close(p2[1]);
        wait(0);
    }
    exit(0);
}

int
main(){
    int p1[2];
    // int p2[2];
    int buf[1];
    if(pipe(p1) < 0){
        exit(1);
    }
    // if(pipe(p2) < 0){
    //     exit(1);
    // }
    for(int i=2; i<=35; ++i){
        buf[0] = i;
        write(p1[1], buf, sizeof(buf));
    }
    // primes(p1, p2);
    primes1(p1);
    exit(0);
}