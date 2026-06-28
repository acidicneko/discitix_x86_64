#include <stdio.h>
#include <string.h>

#define MAX_LINES 1024
#define MAX_LINE  256

char lines[MAX_LINES][MAX_LINE];
int line_count = 0;

void load_file(const char *path)
{
    FILE *fp = fopen(path, "r");

    if (!fp)
        return; // file doesn't exist yet

    while (line_count < MAX_LINES &&
           fgets(lines[line_count], MAX_LINE, fp))
    {
        lines[line_count][strcspn(lines[line_count], "\n")] = '\0';
        line_count++;
    }

    fclose(fp);
}

void save_file(const char *path)
{
    FILE *fp = fopen(path, "w");

    if (!fp) {
        printf("Failed to save file\n");
        return;
    }

    for (int i = 0; i < line_count; i++)
        fprintf(fp, "%s\n", lines[i]);

    fclose(fp);
}

void show_file(void)
{
    printf("\n");

    for (int i = 0; i < line_count; i++)
        printf("%3d | %s\n", i + 1, lines[i]);

    printf("\n");
}

void append_line(const char *line)
{
    if (line_count >= MAX_LINES)
        return;

    strncpy(lines[line_count], line, MAX_LINE - 1);
    lines[line_count][MAX_LINE - 1] = '\0';
    line_count++;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: edit <file>\n");
        return 1;
    }

    load_file(argv[1]);

    char line[MAX_LINE];

    printf("Simple Editor\n");
    printf(":w = save\n");
    printf(":p = print\n");
    printf(":q = quit\n\n");

    while (1) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin))
            break;

        line[strcspn(line, "\n")] = '\0';

        if (!strcmp(line, ":q"))
            break;

        if (!strcmp(line, ":w")) {
            save_file(argv[1]);
            printf("saved\n");
            continue;
        }

        if (!strcmp(line, ":p")) {
            show_file();
            continue;
        }

        append_line(line);
    }

    return 0;
}
