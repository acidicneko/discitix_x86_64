#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Simple argument parser - splits command line by spaces
// Returns argc, fills argv array (must have room for at least max_args)
static int parse_args(char *cmdline, char **argv, int max_args) {
    int argc = 0;
    char *p = cmdline;
    
    while (*p && argc < max_args - 1) {
        // Skip leading whitespace
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        
        // Start of argument
        argv[argc++] = p;
        
        // Find end of argument
        while (*p && *p != ' ' && *p != '\t') p++;
        
        // Null-terminate if not end of string
        if (*p) {
            *p++ = '\0';
        }
    }
    
    argv[argc] = (char*)0;  // NULL terminator
    return argc;
}

int main(int argc, char** argv){
    (void)argc; (void)argv;
    char buffer[128];
    print("Simple Shell (type 'exit' to quit)\n");
    while (1){
        print(">> ");
        if (!gets(buffer, sizeof(buffer))){
            break;
        }
        // Remove newline if present
        int len = 0;
        while (buffer[len] && buffer[len] != '\n') len++;
        buffer[len] = '\0';
        
        if (buffer[0] == '\0') {
            continue;
        }
        
        if (strcmp(buffer, "exit") == 0){
            break;
        } else if(strcmp(buffer, "clear") == 0){
            print("\033[2J\033[H");
            continue;
        }
        
        // Parse command line into arguments
        char *args[16];
        int nargs = parse_args(buffer, args, 16);
        
        if (nargs == 0) {
            continue;
        }
        
        // Build program path from first argument
        char prog_path[64] = "/";
        int i = 0;
        while (args[0][i] && i < 60) {
            prog_path[i + 1] = args[0][i];
            i++;
        }
        prog_path[i + 1] = '\0';
        
        // Spawn the process
        int pid = spawn(prog_path, args);
        if (pid < 0) {
            print("command not found: ");
            print(args[0]);
            print("\n");
            continue;
        }
        
        // Wait for child
        int status;
        waitpid(pid, &status, 0);
    }    
    return 0;
}