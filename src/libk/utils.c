#include <drivers/serial.h>
#include <init/limine_req.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <mm/pmm.h>

void sysfetch() {
  /* ASCII art + info */
  printf("\033[34m '---' \t\033[1;37mKernel: \033[0mDiscitix\n");
  printf("\033[34m (O,O) \t\033[1;37mBuild:  \033[0m%s\n", __DATE__);
  printf("\033[34m /)_)\t\033[1;37mMemory: \033[0m%lu/%lu MB\n",
         get_free_physical_memory() / 1024 / 1024,
         get_total_physical_memory() / 1024 / 1024);
  printf("\033[34m  \"\"  \n\033[0m\n");

  /* Color palette */
  printf("\033[40m  \033[41m  \033[42m  \033[43m  \033[44m  \033[45m  \033[46m "
         " \033[47m  \n");
  printf("\033[21;40m  \033[21;41m  \033[21;42m  \033[21;43m  \033[21;44m  "
         "\033[21;45m  \033[21;46m  \033[21;47m  \033[0m\n");
}

void log(int status, char *fmt, ...) {
  if (status == 1)
    printf("\033[32m[INFO] \033[0m");
  else
    printf("\033[31m[ERROR] \033[0m");
  va_list args;
  va_start(args, fmt);
  __vsprintf__(fmt, args, putchar, puts);
  va_end(args);
}

void dbgln(char *fmt, ...) {
  if (!is_serial_initialized())
    return;
  va_list args;
  va_start(args, fmt);
  __vsprintf__(fmt, args, serial_putchar, serial_puts);
  va_end(args);
}

char *kernel_argv[128];
int kernel_argc = 0;

void init_arg_parser() {
  char *cmdline = (char *)kernel_file_request.response->kernel_file->cmdline;
  if (cmdline != NULL) {
    kernel_argv[kernel_argc] = strtok(cmdline, " ");
    while (kernel_argv[kernel_argc]) {
      kernel_argc++;
      kernel_argv[kernel_argc] = strtok(0, " ");
    }
  } else {
    kernel_argv[kernel_argc] = NULL;
  }
}

int arg_exist(char *arg) {
  for (int i = 0; i < kernel_argc; i++) {
    if (!strcmp(arg, kernel_argv[i]))
      return 1;
  }
  return 0;
}
