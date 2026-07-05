#ifndef DAEMONIZE_H
#define DAEMONIZE_H

#define DA_NO_CHDIR          01   /* INFO: Don't change current working directory to root. */
#define DA_NO_CLOSE_FILES    02   /* INFO: Don't close all open files. */
#define DA_NO_REOPEN_STD_FDS 04   /* INFO: Don't reopen stdin, stdout, and stderr to /dev/null */
#define DA_NO_UNMASK         010  /* INFO: Don't do a unmask(O) */
#define DA_MAX_CLOSE         8192 /* INFO: Maximum file descriptors to close if sysconf(_SC_OPEN_MAX) is indeterminate */

/* INFO: Turns the process into a daemon. Returns 0 on success, -1 on failure. */
int daemonize(int flags);

#endif
