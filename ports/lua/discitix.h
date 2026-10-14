/* Force-included into every Lua source: enables the POSIX features the
   Discitix libc supports (full LUA_USE_POSIX also needs sigsetjmp and
   flockfile, which it doesn't have). */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define l_popen(L,c,m)		(fflush(NULL), popen(c,m))
#define l_pclose(L,file)	(pclose(file))

#define l_inspectstat(stat,what)  \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
   else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }

#define lua_stdin_is_tty()	isatty(0)
