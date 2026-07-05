#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "../include/log.h"

/* INFO: Populates the provided time buffer using Common Log Format. */
static void get_clftime(char *timebuf, int size) {
  time_t time_now = time(NULL);
  struct tm *time_local;
  time_local = localtime(&time_now);
  strftime(timebuf, size, "%d/%b/%Y:%H:%M:%S %z", time_local); 
}

/* INFO: Populates the provided time buffer using a regular format. */
static void get_regtime(char *timebuf, int size) {
  time_t time_now = time(NULL);
  struct tm *time_local;
  time_local = localtime(&time_now);
  strftime(timebuf, size, "%Y-%m-%d %H:%M:%S", time_local); 
}

/* INFO: Converts the provided log_level_t value to a string. Returns a static string literal. */
static char *get_levelstr(enum log_level_t level) {
  switch (level) {
    case LOG_TRACE: return "TRACE";
    case LOG_DEBUG: return "DEBUG";
    case LOG_INFO: return "INFO";
    case LOG_WARNING: return "WARNING";
    case LOG_ERROR: return "ERROR";
    case LOG_FATAL: return "FATAL";
    default: return "UNDEFINED";
  }
}

int log_access(char *file, char *ipbuf, char *request, int status, int response_size) {
  FILE *fd = fopen(file, "a");
  char log[LOG_SIZE_MAX] = {0};
  char timebuf[128] = {0};
  get_clftime(timebuf, sizeof(timebuf));
  snprintf(log, sizeof(log), "%s - - [%s] %s %d %d\n", ipbuf, timebuf, request, status, response_size);
  size_t count = strlen(log);

  if (fwrite(log, sizeof(char), count, fd) < count) {
    perror("(fwrite)");
    return EXIT_FAILURE;
  }

  fclose(fd);
  return EXIT_SUCCESS;
}

int log_message(char *file, enum log_level_t level, const char *fmt, ...) {
  FILE *fd = fopen(file, "a");
  char log[LOG_SIZE_MAX] = {0};
  char logbuf[LOG_SIZE_MAX/2] = {0};
  char timebuf[128] = {0};
  get_regtime(timebuf, sizeof(timebuf));

  va_list args;
  va_start(args, fmt);
  vsnprintf(logbuf, sizeof(logbuf), fmt, args);
  va_end(args);

  snprintf(log, sizeof(log), "[%s] %s: %s\n", timebuf, get_levelstr(level), logbuf);
  size_t count = strlen(log);

  if (fwrite(log, sizeof(char), count, fd) < count) {
    perror("(fwrite)");
    return EXIT_FAILURE;
  }

  fclose(fd);
  return EXIT_SUCCESS;
}


