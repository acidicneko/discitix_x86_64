#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file_or_directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *target = argv[1];

    // remove() is standard C. Under the hood in Newlib, it will 
    // attempt to call your _unlink() stub.
    if (remove(target) == 0) {
        printf("Successfully removed '%s'\n", target);
        return EXIT_SUCCESS;
    } else {
        // If it fails, perror will read 'errno' and print the exact reason
        perror("rm failed");
        return EXIT_FAILURE;
    }
}
