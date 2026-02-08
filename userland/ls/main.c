#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>

/* ANSI colors */
#define CLR_RESET "\033[0m"
#define CLR_BLUE  "\033[34m"
#define CLR_GREEN "\033[32m"
#define CLR_CYAN  "\033[36m"

static void printc(char c) {
    char s[2] = { c, 0 };
    print(s);
}

static void print_mode(mode_t m) {
    if (S_ISDIR(m))      printc('d');
    else if (S_ISCHR(m)) printc('c');
    else if (S_ISBLK(m)) printc('b');
    else if (S_ISLNK(m)) printc('l');
    else                 printc('-');

    printc((m & S_IRUSR) ? 'r' : '-');
    printc((m & S_IWUSR) ? 'w' : '-');
    printc((m & S_IXUSR) ? 'x' : '-');

    printc((m & S_IRGRP) ? 'r' : '-');
    printc((m & S_IWGRP) ? 'w' : '-');
    printc((m & S_IXGRP) ? 'x' : '-');

    printc((m & S_IROTH) ? 'r' : '-');
    printc((m & S_IWOTH) ? 'w' : '-');
    printc((m & S_IXOTH) ? 'x' : '-');
}

static void print_num_width(unsigned int n, int width) {
    char buf[16];
    int i = 0;

    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }

    while (i < width)
        buf[i++] = ' ';

    while (i--)
        printc(buf[i]);
}

static void print_name_colored(const char *name, mode_t mode) {
    if (S_ISDIR(mode))
        print(CLR_BLUE);
    else if (S_ISLNK(mode))
        print(CLR_CYAN);
    else if (mode & S_IXOTH)
        print(CLR_GREEN);

    print(name);
    print(CLR_RESET);
}

int main(int argc, char *argv[]) {
    const char *path = (argc >= 2) ? argv[1] : ".";

    DIR *dir = opendir(path);
    if (!dir) {
        print("ls: cannot open ");
        println(path);
        return 1;
    }

    struct dirent *de;
    while ((de = readdir(dir))) {
        char full[512];
        int i = 0;

        /* base path */
        for (; path[i] && i < 500; i++)
            full[i] = path[i];

        if (i && full[i - 1] != '/')
            full[i++] = '/';

        /* filename */
        for (int j = 0; de->d_name[j] && i < 510; j++)
            full[i++] = de->d_name[j];

        full[i] = 0;

        struct stat st;
        if (stat(full, &st) != 0)
            continue;

        print_mode(st.st_mode);
        print("  ");
        print_num_width(st.st_size, 8);
        print("  ");
        print_name_colored(de->d_name, st.st_mode);
        print("\n");
    }

    closedir(dir);
    return 0;
}
