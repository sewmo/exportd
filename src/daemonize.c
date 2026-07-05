#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "../include/daemonize.h"

/* INFO: Turns the process into a daemon. Returns 0 on success, or -1 on error. */
int daemonize(int flags) {
  int maxfd, fd;

  switch (fork()) {
    case -1: return -1;
    case 0: break;
    default: _exit(EXIT_SUCCESS);
  }

  if (setsid() == -1) 
    return -1;

  switch (fork()) {
    case -1: return -1;
    case 0: break;
    default: _exit(EXIT_SUCCESS);
  }

  if (!(flags & DA_NO_UNMASK))
    umask(0);

  if (!(flags & DA_NO_CHDIR))
    chdir("/");

  if (!(flags & DA_NO_CLOSE_FILES)) {
    maxfd = sysconf(_SC_OPEN_MAX);
    if (maxfd == -1)
      maxfd = DA_MAX_CLOSE;

    for (fd = 0; fd < maxfd; fd++)
      close(fd);
  }

  if (!(flags & DA_NO_REOPEN_STD_FDS)) {
    close(STDIN_FILENO);
    fd = open("/dev/null", O_RDWR);

    if (fd != STDIN_FILENO)
      return -1;
    if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
      return -1;
    if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
      return -1;
  }

  return 0;
}
