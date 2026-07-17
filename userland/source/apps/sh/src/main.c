#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define GREEN "\033[1;32m"
#define RED   "\033[1;31m"
#define RESET "\033[0m"

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_ENV 128
#define MAX_PATHS 64
#define MAX_PATH_LEN 512

typedef struct {
    char *dirs[MAX_PATHS];
    int count;
} path_table_t;

static path_table_t paths;
static char *envp_new[MAX_ENV];
static int envc = 0;
static int last_status = 0;

static void trim(char *s){
    while(isspace((unsigned char)*s))
        memmove(s,s+1,strlen(s));
    size_t n=strlen(s);
    while(n && isspace((unsigned char)s[n-1]))
        s[--n]=0;
}

static void parse_env(const char *file){
    FILE *fp=fopen(file,"r");
    if(!fp) return;
    char line[512];

    while(fgets(line,sizeof(line),fp)){
        trim(line);
        if(!line[0]||line[0]=='#') continue;

        char *eq=strchr(line,'=');
        if(!eq) continue;
        *eq=0;

        char *key=line;
        char *val=eq+1;

        trim(key);
        trim(val);

        if((*val=='"'||*val=='\'') && strlen(val)>=2){
            size_t l=strlen(val);
            if(val[l-1]==val[0]){
                val[l-1]=0;
                val++;
            }
        }

        char *entry=malloc(strlen(key)+strlen(val)+2);
        sprintf(entry,"%s=%s",key,val);
        envp_new[envc++]=entry;

        if(strcmp(key,"PATH")==0){
            char *copy=strdup(val);
            char *tok=strtok(copy,":");
            while(tok && paths.count<MAX_PATHS){
                paths.dirs[paths.count++]=strdup(tok);
                tok=strtok(NULL,":");
            }
            free(copy);
        }
    }

    envp_new[envc]=NULL;
    fclose(fp);
}

static int parse(char *line,char *argv[]){
    int argc=0;
    while(*line){
        while(isspace((unsigned char)*line)) line++;
        if(!*line) break;

        if(*line=='"'){
            line++;
            argv[argc++]=line;
            while(*line && *line!='"') line++;
            if(*line) *line++=0;
        }else{
            argv[argc++]=line;
            while(*line && !isspace((unsigned char)*line)) line++;
            if(*line) *line++=0;
        }
        if(argc==MAX_ARGS-1) break;
    }
    argv[argc]=NULL;
    return argc;
}

static int exists_exec(const char *p){
    struct stat st;
    return stat(p,&st)==0;
}

static int find_program(const char *prog,char *out){
    if(strchr(prog,'/')){
        if(exists_exec(prog)){
            strcpy(out,prog);
            return 0;
        }
        return -1;
    }

    for(int i=0;i<paths.count;i++){
        snprintf(out,MAX_PATH_LEN,"%s/%s",paths.dirs[i],prog);
        if(exists_exec(out))
            return 0;
    }
    return -1;
}

static void prompt(void){
    char cwd[MAX_PATH_LEN];
    if(!getcwd(cwd,sizeof(cwd)))
        strcpy(cwd,"?");

    printf(GREEN "%s " RESET,cwd);
    printf("%s$ " RESET,last_status?RED:GREEN);
    fflush(stdout);
}

int main(void){
    parse_env("/etc/environment");

    char line[MAX_LINE];

    while(1){
        prompt();

        if(!fgets(line,sizeof(line),stdin))
            break;

        line[strcspn(line,"\n")]=0;

        char *argv[MAX_ARGS];
        int argc=parse(line,argv);
        if(argc==0) continue;

        if(strcmp(argv[0],"exit")==0)
            break;

        if(strcmp(argv[0],"cd")==0){
            const char *dir=(argc>1)?argv[1]:"/";
            if(chdir(dir)==0)
                last_status=0;
            else{
                perror("cd");
                last_status=1;
            }
            continue;
        }
        if(strcmp(argv[0], "clear")==0){
          printf("\033[2J\033[H");
          continue;
        }

        char full[MAX_PATH_LEN];
        if(find_program(argv[0],full)!=0){
            fprintf(stderr,"%s: command not found\n",argv[0]);
            last_status=127;
            continue;
        }

        pid_t pid=fork();
        if(pid<0){
            perror("fork");
            last_status=1;
            continue;
        }

        if(pid==0){
            execve(full,argv,envp_new);
            perror("execve");
            _exit(127);
        }

        int st;
        waitpid(pid,&st,0);

        if(WIFEXITED(st))
            last_status=WEXITSTATUS(st);
        else
            last_status=1;
    }

    return 0;
}

