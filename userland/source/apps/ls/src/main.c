#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Your custom dirent declarations */

typedef struct {
    int fd;
    char buf[1024];
    size_t buf_pos;
    size_t buf_len;
} DIR;

struct dirent {
    unsigned long d_ino;
    unsigned char d_type;
    char d_name[256];
};

extern DIR *opendir(const char *name);
extern struct dirent *readdir(DIR *dirp);
extern int closedir(DIR *dirp);

#define CLR_RESET "\033[0m"
#define CLR_BLUE  "\033[1;34m"
#define CLR_CYAN  "\033[1;36m"
#define CLR_GREEN "\033[1;32m"

static void print_mode(mode_t mode)
{
    char p[11];

    p[0] = S_ISDIR(mode)  ? 'd' :
           S_ISLNK(mode)  ? 'l' :
           S_ISCHR(mode)  ? 'c' :
           S_ISBLK(mode)  ? 'b' :
           S_ISFIFO(mode) ? 'p' :
           S_ISSOCK(mode) ? 's' : '-';

    p[1] = (mode & S_IRUSR) ? 'r' : '-';
    p[2] = (mode & S_IWUSR) ? 'w' : '-';
    p[3] = (mode & S_IXUSR) ? 'x' : '-';

    p[4] = (mode & S_IRGRP) ? 'r' : '-';
    p[5] = (mode & S_IWGRP) ? 'w' : '-';
    p[6] = (mode & S_IXGRP) ? 'x' : '-';

    p[7] = (mode & S_IROTH) ? 'r' : '-';
    p[8] = (mode & S_IWOTH) ? 'w' : '-';
    p[9] = (mode & S_IXOTH) ? 'x' : '-';

    p[10] = '\0';

    printf("%s", p);
}

static const char *file_color(mode_t mode)
{
    if (S_ISDIR(mode))
        return CLR_BLUE;

    if (S_ISLNK(mode))
        return CLR_CYAN;

    if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
        return CLR_GREEN;

    return "";
}

static void format_size(long size, char *buf, size_t buflen)
{
    const char *units[] = {
        "B", "K", "M", "G", "T"
    };

    int unit = 0;
    double s = (double)size;

    while (s >= 1024.0 && unit < 4) {
        s /= 1024.0;
        unit++;
    }

    if (unit == 0)
        snprintf(buf, buflen,
                 "%ld%s",
                 size,
                 units[unit]);
    else
        snprintf(buf, buflen,
                 "%.1f%s",
                 s,
                 units[unit]);
}

static void list_dir(
    const char *path,
    int long_fmt,
    int show_all,
    int human)
{
    DIR *dir = opendir(path);

    if (!dir) {
        fprintf(stderr,
                "ls: cannot open %s\n",
                path);
        return;
    }

    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {

        if (!show_all &&
            ent->d_name[0] == '.')
            continue;

        char fullpath[512];
        struct stat st;

        snprintf(fullpath,
                 sizeof(fullpath),
                 "%s/%s",
                 path,
                 ent->d_name);

        if (stat(fullpath, &st) < 0)
            continue;

        if (long_fmt) {

            char sizebuf[16];

            print_mode(st.st_mode);

            if (human)
                format_size(
                    st.st_size,
                    sizebuf,
                    sizeof(sizebuf));
            else
                snprintf(sizebuf,
                         sizeof(sizebuf),
                         "%ld",
                         (long)st.st_size);

            printf(" %8s ",
                   sizebuf);
        }

        printf("%s%s%s\n",
               file_color(st.st_mode),
               ent->d_name,
               CLR_RESET);
    }

    closedir(dir);
}

int main(int argc, char **argv)
{
    int long_fmt = 0;
    int show_all = 0;
    int human = 0;

    const char *path = ".";

    for (int i = 1; i < argc; i++) {

        if (argv[i][0] == '-') {

            for (char *p = argv[i] + 1;
                 *p;
                 p++) {

                switch (*p) {

                case 'l':
                    long_fmt = 1;
                    break;

                case 'a':
                    show_all = 1;
                    break;

                case 'h':
                    human = 1;
                    break;

                default:
                    fprintf(stderr,
                            "ls: unknown option -%c\n",
                            *p);
                    return 1;
                }
            }
        } else {
            path = argv[i];
        }
    }

    list_dir(path,
             long_fmt,
             show_all,
             human);

    return 0;
}
