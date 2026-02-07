#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv){
    (void)argc; (void)argv;
    int tries = 5;
    while (tries-- > 0) {
        print("Enter password: ");
        char buffer[64];
        if (!gets(buffer, sizeof(buffer))){
            print("Error reading input\n");
            return 1;
        }
        
        if (strcmp(buffer, "ayush123") == 0){
            print("Authentication successful!\n");
            char* args[] = {"sh", NULL};
            spawn("/sh", args);
            return 0;
        } else {
            print("Incorrect password. Try again.\n");
        }
    }
    return 0;
}