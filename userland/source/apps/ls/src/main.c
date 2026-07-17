#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Custom dirent declarations */
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

typedef struct {
    char name[256];
    mode_t mode;
    off_t size;
} FileEntry;

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
    if (S_ISDIR(mode)) return CLR_BLUE;
    if (S_ISLNK(mode)) return CLR_CYAN;
    if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) return CLR_GREEN;
    return "";
}

static void format_size(long size, char *buf, size_t buflen)
{
    const char *units[] = { "B", "K", "M", "G", "T" };
    int unit = 0;
    double s = (double)size;

    while (s >= 1024.0 && unit < 4) {
        s /= 1024.0;
        unit++;
    }

    if (unit == 0)
        snprintf(buf, buflen, "%ld%s", size, units[unit]);
    else
        snprintf(buf, buflen, "%.1f%s", s, units[unit]);
}

static int get_terminal_width()
{
    return 80;
}

// Custom sort comparison function
static int compare_entries(const void *a, const void *b)
{
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;

    int a_is_dir = S_ISDIR(fa->mode);
    int b_is_dir = S_ISDIR(fb->mode);

    // Rule 1: Directories come first
    if (a_is_dir && !b_is_dir) return -1;
    if (!a_is_dir && b_is_dir) return 1;

    // Rule 2: Both are the same category, sort alphabetically (case-insensitive)
    return strcasecmp(fa->name, fb->name);
}

static void list_dir(const char *path, int long_fmt, int show_all, int human)
{
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "ls: cannot open %s\n", path);
        return;
    }

    struct dirent *ent;
    FileEntry *entries = NULL;
    int count = 0;
    int capacity = 0;
    int max_len = 0;

    while ((ent = readdir(dir)) != NULL) {
        if (!show_all && ent->d_name[0] == '.')
            continue;

        char fullpath[512];
        struct stat st;
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);

        if (stat(fullpath, &st) < 0)
            continue;

        if (count >= capacity) {
            capacity = capacity == 0 ? 64 : capacity * 2;
            entries = realloc(entries, capacity * sizeof(FileEntry));
        }

        strncpy(entries[count].name, ent->d_name, sizeof(entries[count].name));
        entries[count].mode = st.st_mode;
        entries[count].size = st.st_size;

        int len = strlen(ent->d_name);
        if (len > max_len) {
            max_len = len;
        }
        count++;
    }
    closedir(dir);

    if (count == 0) {
        free(entries);
        return;
    }

    // Sort entries: Directories first, then alphabetically
    qsort(entries, count, sizeof(FileEntry), compare_entries);

    if (long_fmt) {
        for (int i = 0; i < count; i++) {
            char sizebuf[16];
            print_mode(entries[i].mode);

            if (human)
                format_size(entries[i].size, sizebuf, sizeof(sizebuf));
            else
                snprintf(sizebuf, sizeof(sizebuf), "%ld", (long)entries[i].size);

            printf(" %8s %s%s%s\n", sizebuf, file_color(entries[i].mode), entries[i].name, CLR_RESET);
        }
    } else {
        int term_width = get_terminal_width();
        int col_width = max_len + 2;
        int cols = term_width / col_width;
        if (cols <= 0) cols = 1;

        int rows = (count + cols - 1) / cols;

        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                int idx = c * rows + r;
                if (idx < count) {
                    if ((c + 1) * rows + r >= count) {
                        printf("%s%s%s", file_color(entries[idx].mode), entries[idx].name, CLR_RESET);
                    } else {
                        printf("%s%-*s%s", file_color(entries[idx].mode), col_width, entries[idx].name, CLR_RESET);
                    }
                }
            }
            printf("\n");
        }
    }

    free(entries);
}

int main(int argc, char **argv)
{
    int long_fmt = 0;
    int show_all = 0;
    int human = 0;
    const char *path = ".";

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (char *p = argv[i] + 1; *p; p++) {
                switch (*p) {
                case 'l': long_fmt = 1; break;
                case 'a': show_all = 1; break;
                case 'h': human = 1; break;
                default:
                    fprintf(stderr, "ls: unknown option -%c\n", *p);
                    return 1;
                }
            }
        } else {
            path = argv[i];
        }
    }

    list_dir(path, long_fmt, show_all, human);
    return 0;
}
