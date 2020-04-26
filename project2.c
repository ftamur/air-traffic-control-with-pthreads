#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pthread_sleep.c"

int read_command_line(int argc, char *argv[], float *p, int *s);

int main(int argc, char *argv[]) {

    int s;
    float p;
    read_command_line(argc, argv, &p, &s);
    
    return 0;
}


int read_command_line(int argc, char *argv[], float *p, int *s) {

    if (argc != 5){
        puts("Inappropriate argument count!!!");
        return 0;
    }

    if (strcmp(argv[1], "-s")) {
        puts("Specify -s as first argument for the simulation!!!");
        return 0;
    }

    if (strcmp(argv[3], "-p")) {
        puts("Specify -s as second argument for the simulation!!!");
        return 0;
    }

    *s = atoi(argv[2]);
    *p = atof(argv[4]);

    if (*s == 0 || *p == 0) {
        puts("Specify integer values for the -s or -p values!!!");
        return 0;
    }

    return 1;

}


