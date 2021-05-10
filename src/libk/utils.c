#include "libk/stdio.h"
#include "libk/utils.h"

void sysfetch(){
    printf("\033[34m ____  \n");
    printf("\033[34m|  _ \\\t\033[1;37mKernel: \033[0mDiscitix\n");
    printf("\033[34m| | | |\t\033[1;37mBuild: \033[0m%s\n", __DATE__);
    printf("\033[34m| |_| |\n");
    printf("\033[34m|____/\n\033[0m\n");

    printf("\033[40m  \033[41m  \033[42m  \033[43m  \033[44m  \033[45m  \033[46m  \033[47m  \n");
    printf("\033[21;40m  \033[21;41m  \033[21;42m  \033[21;43m  \033[21;44m  \033[21;45m  \033[21;46m  \033[21;47m  \033[0m\n");
}

void log(int status, char *fmt, ...){
    if(status == 1)
        printf("\033[32m[INFO] \033[0m");
    else
        printf("\033[31m[ERROR] \033[0m");
    va_list args;
    va_start(args, fmt);
    vsprintf(fmt, args);
    va_end(args);
}