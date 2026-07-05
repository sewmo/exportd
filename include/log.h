#ifndef LOG_H
#define LOG_H

#include <stdbool.h>

#define LOG_SIZE_MAX 512

enum log_level_t {
  LOG_TRACE,
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARNING,
  LOG_ERROR,
  LOG_FATAL
};

/* INFO: Intended for logging completed requests with the Common Log Format (CLF).
         A new line will be appended to the message automatically.
         Returns a non-zero value on failure. */
int log_access(char *file, char *ipbuf, char *request, int status, int response_size);

/* INFO: Intended for logging arbitrary messages at designated logging levels. 
         A new line will be appended to the message automatically.
         Returns a non-zero value on failure. */
int log_message(char *file, enum log_level_t level, const char *fmt, ...);

#endif
