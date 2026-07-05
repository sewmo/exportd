#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <poll.h>

#include "../include/server.h"
#include "../include/log.h"

/* 
   TODO: (PARTIAL: Only checks root directory) Check if the program has read permissions on the root directory, and its subdirectories.
   TODO: Support for headers: Host, User-Agent, Accept.
   TODO: Ensure path parser does not allow malicious folder navigation using '..'.
   TODO: More path parser support for special characters.
   TODO: Access logs, error logs, timestamp logs. Example: (127.0.0.1 - - [26/May/2026:18:31:22] "GET /index.html HTTP/1.1" 200 512)
   TODO: Improve MIME type support. Method used for determining MIME type could also be improved?
*/

void server_config(struct config_t *config) {
  config->max_queues = 10;
  config->max_conns = 10;
  config->verbose = false;
  memset(config->rootdir, 0, sizeof(config->rootdir));
  strcpy(config->rootdir, "/srv/exportd/www");
  memset(config->access_log, 0, sizeof(config->access_log));
  strcpy(config->access_log, "/srv/exportd/access.log");
  memset(config->error_log, 0, sizeof(config->error_log));
  strcpy(config->error_log, "/srv/exportd/error.log");
}

/* INFO: Returns a non-zero value on failure. */
static int get_status_line(char *message_status, int status) {
  char *status_desc;
  switch (status) {
    case STATUS_OK: status_desc = "OK"; break;
    case STATUS_BAD_REQUEST: status_desc = "Bad Request"; break;
    case STATUS_FORBIDDEN: status_desc = "Forbidden"; break;
    case STATUS_NOT_FOUND: status_desc = "Not Found"; break;
    case STATUS_NOT_IMPLEMENTED: status_desc = "Not Implemented"; break;
    case STATUS_VERSION_NOT_SUPPORTED: status_desc = "Version Not Supported"; break;
    default: fprintf(stderr, "httpd: Unrecognized status passed %d!\n", status); return EXIT_FAILURE; break;
  }
  sprintf(message_status, "%s %d %s\r\n", HTTP_VERSION, status, status_desc);
  return EXIT_SUCCESS;
} 

/* INFO: Returns the length of the specified file. */
static int get_file_len(char *file, bool binary) {
  FILE *stream;
  if (!binary)
    stream = fopen(file, "r");
  else
    stream = fopen(file, "rb");

  fseek(stream, 0, SEEK_END);
  int len = ftell(stream);
  rewind(stream);

  fclose(stream);

  return len;
}

/* INFO: Returns a non-zero value on failure. */
static int send_all(char *msg, int fd, int len) { 
  size_t bytes_left = len;
  ssize_t bytes_sent;
  const char *ptr = (const char *)msg;
  while (bytes_left > 0) {
    bytes_sent = send(fd, ptr, bytes_left, 0);
    if (bytes_sent < 0) {
      fprintf(stderr, "httpd: Could not send bytes to connected client!\n");
      return EXIT_FAILURE;
    }
    bytes_left -= bytes_sent;
    ptr += bytes_sent;
  }
  return EXIT_SUCCESS;
}

/* INFO: Returns a non-zero value on failure. */
static int send_file(char *file, int fd, bool binary, bool verbose) {
  FILE *stream;
  if (!binary) 
    stream = fopen(file, "r");
  else 
    stream = fopen(file, "rb");

  char buf[8192] = {0};
  int bytes_read; int total = 0;
  if (binary && verbose) printf("httpd: Contents of message body is binary, not printing.\n");

  while ((bytes_read = fread(buf, sizeof(char), sizeof(buf), stream)) > 0) {
    if (!binary && verbose) printf("%s", buf);
    if (verbose) printf("httpd: Bytes read from stream: %d\n", bytes_read);
    send_all(buf, fd, bytes_read);
    total += bytes_read;
  }

  if (ferror(stream) != 0) {
    fprintf(stderr, "httpd: Encountered an error while reading the file stream!\n");
    return EXIT_FAILURE;
  }

  if (verbose) printf("httpd: Total bytes read from stream: %d\n", total);
  fclose(stream);
  return EXIT_SUCCESS;
}

/* INFO: Supplies the appropriate content type given the request path. The binary boolean is set to true if the content type is binary. */
static void get_content_type(char *content_type, char *request_path, bool *binary) {
  char text_types[7][16] = {"html", "css", "js", "javascript", "json", "txt", "csv", };
  char image_types[8][16] = {"ico", "png", "jpg", "jpeg", "gif", "svg", "avif", "webp"};
  char search[32] = {0};
  char type[16] = {0};
  size_t length = sizeof(text_types) / sizeof(text_types[0]);

  for (size_t i = 0; i < length; i++) {
    snprintf(search, sizeof(search), ".%s", text_types[i]);
    if (strstr(request_path, search) != NULL) {
      strcat(type, text_types[i]);
      sprintf(content_type, "text/%s; charset=UTF-8", type);
      *binary = false;
      return;
    }
  }

  length = sizeof(image_types) / sizeof(image_types[0]);
  for (size_t i = 0; i < length; i++) {
    snprintf(search, sizeof(search), ".%s", image_types[i]);
    if (strstr(request_path, search) != NULL) {
      if (strcmp(image_types[i], "ico") == 0)
        strcat(type, "x-icon");
      else strcat(type, image_types[i]);
      sprintf(content_type, "image/%s", type);
      *binary = true;
      return;
    }
  }
}

/* INFO: Converts a string to a method_t enum value. If no match is found, returns -1. */
static enum method_t str_to_method(char *str) {
  if (strcmp(str, "GET") == 0) {
    return HTTP_GET;
  } else if (strcmp(str, "POST") == 0) {
    return HTTP_POST; 
  } else if (strcmp(str, "PUT") == 0) {
    return HTTP_PUT;
  }
  return -1;
}

/* INFO: Returns a non-zero value on failure. */
static int send_http(struct server_t *server, struct config_t *config, int fd, int status, char *request_path, enum method_t request_method) {
  char message[1024] = {0};
  char message_status[64];
  if (get_status_line(message_status, status) != EXIT_SUCCESS)
    return EXIT_FAILURE;
  strcat(message, message_status);

  switch (request_method) {
    case HTTP_GET: {
      bool binary;
      char content_type[64];
      get_content_type(content_type, request_path, &binary);

      if (status != STATUS_OK) {
        if (config->verbose) printf("httpd: Sending HTTP status line %d:\n%s\n", status, message);
        send_all(message, fd, strlen(message));
        return EXIT_SUCCESS;
      }

      int body_length = get_file_len(request_path, binary);

      time_t time_now = time(NULL);
      struct tm *time_gmt;
      time_gmt = gmtime(&time_now);
      char time_buf[128];

      strftime(time_buf, sizeof(time_buf), "%a, %d %b %Y %H:%M:%S GMT", time_gmt);

      char headers[4][256];
      snprintf(headers[0], sizeof(headers[2]), "Date: %s\r\n", time_buf);
      snprintf(headers[1], sizeof(headers[0]), "Content-Length: %d\r\n", body_length);
      snprintf(headers[2], sizeof(headers[1]), "Content-Type: %s\r\n", content_type);
      snprintf(headers[3], sizeof(headers[3]), "Connection: keep-alive\r\n");
      for (int i = 0; i < 4; i++) strcat(message, headers[i]);

      strcat(message, "\r\n");

      if (config->verbose) printf("httpd: Sending HTTP headers and status line:\n%s\n", message);
      if (send_all(message, fd, strlen(message)) != EXIT_SUCCESS)
        return EXIT_FAILURE;

      if (config->verbose) printf("httpd: Sending HTTP message body:\n");
      if (send_file(request_path, fd, binary, config->verbose) != EXIT_SUCCESS)
        return EXIT_FAILURE;
      break;
    }
    case HTTP_POST: {
      break;
    }
    case HTTP_PUT: {
      break;
    }
    default: {
      if (config->verbose) printf("httpd: Could not convert string to method_t enum value.\n");
      log_message(config->error_log, LOG_FATAL, "Failed to obtain valid method_t value, case undefined!\n");
      return EXIT_FAILURE;
    }
  }

   return EXIT_SUCCESS;
}

/* INFO: Returns the appropriate status code in response to the request. Supplies the request_path and request_method fields. The request_path is assumed to be 128 bytes in size. */
static int parse_request(struct server_t *server, struct config_t *config, char *request, char request_path[PATH_LEN], char request_method[64]) {
  char tokens[3][64];
  
  int i = 0; int j = 0; int k = 0;
  while (request[i] != '\r') {
    if (request[i] == ' ') {
      if (k >= 2) {
        if (config->verbose) printf("httpd: Encountered more than 3 tokens while parsing client's request! Returning %d.\n", STATUS_BAD_REQUEST);
        return STATUS_BAD_REQUEST; 
      } 
      tokens[k][j] = '\0';
      i++; k++; j = 0;
      continue; 
    }
    tokens[k][j] = request[i];
    i++; j++;
  }
  tokens[2][j] = '\0';

  strcat(request_method, tokens[0]);

  /*
  if (strstr(request, "Host:") == NULL &&) {
    if (config->verbose) printf("httpd: Client omitted sending a host header! Returning %d.\n", STSATUS_BAD_REQUEST);
    return STATUS_BAD_REQUEST;
  }
  */

  if (strcmp(tokens[2], HTTP_VERSION) != 0 && strcmp(tokens[2], "HTTP/1.1") != 0) {
    if (config->verbose) printf("httpd: Client uses a different version that is not supported by the server! Returning %d\n", STATUS_VERSION_NOT_SUPPORTED);
    return STATUS_VERSION_NOT_SUPPORTED;
  }

  if (strcmp(tokens[0], "GET") != 0 && strcmp(tokens[0], "POST") != 0) {
    if (config->verbose) printf("httpd: Method requested by client is not implemented! Returning %d\n", STATUS_NOT_IMPLEMENTED);
    log_message(config->error_log, LOG_INFO, "The client's HTTP method has not yet been implemented! Returning status %d", STATUS_NOT_IMPLEMENTED);
    return STATUS_NOT_IMPLEMENTED;
  }

  if (strcmp(tokens[1], "/") == 0) {
    snprintf(request_path, PATH_LEN, "%s/index.html", config->rootdir);
  } else {
    snprintf(request_path, PATH_LEN, "%s%s", config->rootdir, tokens[1]);
  }

  char root_path[PATH_LEN] = {0};
  char resolved_path[PATH_LEN] = {0};

  if (realpath(request_path, resolved_path) == NULL) {
    if (config->verbose) printf("httpd: The client's request path does not exist! Returning %d\n", STATUS_NOT_FOUND);
    log_message(config->error_log, LOG_INFO, "The client's requested file could not be found! Returning status %d", STATUS_NOT_FOUND);
    return STATUS_NOT_FOUND;
  }

  realpath(config->rootdir, root_path);
  size_t root_len = strlen(root_path);

  if ((strncmp(resolved_path, root_path, root_len) != 0) || (resolved_path[root_len] != '/' && resolved_path[root_len] != '\0')) {
    if (config->verbose) printf("httpd: The client's request path is possibly malicious! Returning %d\n", STATUS_FORBIDDEN);
    log_message(config->error_log, LOG_INFO, "Possibly malicious request sent by client! Returning %d", STATUS_FORBIDDEN);
    return STATUS_FORBIDDEN;
  }

  if (access(request_path, R_OK) != 0) {
    if (config->verbose) printf("httpd: The client's request target cannot be read by the process! Returning %d\n", STATUS_FORBIDDEN);
    log_message(config->error_log, LOG_WARNING, "The SERVER lacks read permissions for the client's requested file! Returning status %d", STATUS_FORBIDDEN);
    return STATUS_FORBIDDEN;
  }

  return STATUS_OK;
}

int server_init(struct server_t *server, struct config_t *config) {
  server->port = config->port;

  DIR* dir = opendir(config->rootdir);
  if (dir) {
    if (config->verbose) printf("httpd: Root directory '%s' exists!\n", config->rootdir);
    closedir(dir);
  } else if (ENOENT == errno) {
    fprintf(stderr, "httpd: The root directory '%s' could not be found!\n", config->rootdir);
    return EXIT_FAILURE;
  } else if (EACCES == errno) {
    fprintf(stderr, "httpd: The root directory '%s' exists, but the program lacks permissions to access it!\n", config->rootdir);
    return EXIT_FAILURE;
  } else {
    fprintf(stderr, "httpd: Failed to open the root directory '%s'\n", config->rootdir);
    return EXIT_FAILURE;
  }

  log_message(config->error_log, LOG_INFO, "Root directory '%s' validated", config->rootdir);

  server->addr.sin_family = AF_INET;
  server->addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server->addr.sin_port = htons(server->port);

  server->socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server->socket < 0) {
    fprintf(stderr, "httpd: Could not get a socket descriptor for the listening socket!\n");
    perror("(socket)");
    return EXIT_FAILURE;
  }
  if (config->verbose) printf("httpd: Created a socket file descriptor!\n");
  log_message(config->error_log, LOG_DEBUG, "Created socket file descriptor %d", server->socket);

  if (bind(server->socket, (struct sockaddr *)&server->addr, sizeof(server->addr)) < 0) {
    fprintf(stderr, "httpd: Could not bind the socket to port %d! Other processes could be listening on port %d, or the executable may lack root privileges!\n", config->port, config->port);
    perror("(bind)");
    return EXIT_FAILURE;
  }
  if (config->verbose) printf("httpd: Bound socket file descriptor to port %d on all interfaces!\n", config->port);
  log_message(config->error_log, LOG_DEBUG, "Bound socket file descriptor %d to :%d on all interfaces", server->socket, server->port);

  if (listen(server->socket, config->max_queues) < 0) {
    fprintf(stderr, "httpd: Could not mark socket as listening!\n");
    perror("(listen)");
    return EXIT_FAILURE;
  }
  log_message(config->error_log, LOG_DEBUG, "Socket file descriptor %d is now listening on :%d", server->socket, server->port);

  if (config->verbose) printf("httpd: Server is ready to accept clients!\n");
  return EXIT_SUCCESS;
}

int server_loop(struct server_t *server, struct config_t *config) {
  struct pollfd pfds[config->max_conns];
  char *fd_ip[config->max_conns];
  char fd_path[config->max_conns][PATH_LEN];
  int fd_port[config->max_conns];
  int fd_status[config->max_conns];
  enum method_t fd_method[config->max_conns];

  for (int i = 0; i < config->max_conns; i++) {
    pfds[i].events = POLLIN | POLLOUT;
    pfds[i].revents = 0;
    pfds[i].fd = -1;
    fd_ip[i] = NULL;
    fd_port[i] = -1;
    fd_status[i] = -1;
    fd_method[i] = -1;
    memset(fd_path[i], 0, sizeof(fd_path[i]));
  }
  pfds[0].fd = server->socket;

  struct sockaddr_in sockaddr;
  socklen_t socklen = sizeof(sockaddr);
  int sockfd, rv, nfds, conns, status;
  nfds = config->max_conns;
  conns = 0;

  char request[2048] = {0};
  char request_path[PATH_LEN] = {0};
  char request_method[64] = {0};

  if (config->verbose) printf("httpd: Starting client accept loop!\n");
  while (true) {
    rv = poll(pfds, nfds, -1);
    if (rv == -1) {
      fprintf(stderr, "httpd: Poll failed.\n");
      perror("(poll)");
      return EXIT_FAILURE;
    }

    // New connection pending;
    if (pfds[0].revents & POLLIN) {
      sockfd = accept(server->socket, (struct sockaddr *)&sockaddr, &socklen);
      if (sockfd < 0) {
        fprintf(stderr, "httpd: Could not get a socket file descriptor for connected client!\n");
        perror("(accept)");
        return EXIT_FAILURE;
      }

      conns++;
      pfds[conns].fd = sockfd;
      pfds[conns].revents = 0;
      pfds[conns].events = POLLIN | POLLOUT;

      fd_ip[conns] = inet_ntoa(sockaddr.sin_addr);
      fd_port[conns] = ntohs(sockaddr.sin_port);

      if (config->verbose) { 
        printf("httpd: New client connected from IP %s and port %d\n", fd_ip[conns], fd_port[conns]);
        printf("httpd: Waiting to receive HTTP requests from the client.\n");
      }
    }

    // Message checks;
    for (int i = 1; i <= conns; i++) {
      if (pfds[i].fd == -1) continue;

      // Message from client pending;
      if (pfds[i].revents & POLLIN) {
        if (config->verbose)
          printf("httpd: Receiving message from client #%d.\n", i);
        memset(request, 0, sizeof(request));
        rv = recv(pfds[i].fd, request, sizeof(request), 0);
        if (rv == -1) {
          fprintf(stderr, "httpd: Error occured while receiving message from client #%d!\n", i);
          perror("(recv)");
          return EXIT_FAILURE;
        } else if (rv == 0) {
          if (config->verbose) 
            printf("httpd: Client #%d disconnected.\n", i);
          for (int j = i; j < conns; j++) {
            pfds[j].fd = pfds[j+1].fd;
            pfds[j].events = pfds[j+1].events;
            pfds[j].revents = pfds[j+1].revents;
            fd_ip[j] = fd_ip[j+1];
            fd_port[j] = fd_port[j+1];
            fd_status[j] = fd_status[j+1];
            fd_method[j] = fd_method[j+1];
            strcpy(fd_path[j+1], fd_path[j]);
          }
          pfds[conns].fd = -1;
          pfds[conns].events = POLLIN | POLLOUT;
          pfds[conns].revents = 0;
          fd_ip[conns] = NULL;
          fd_port[conns] = -1;
          fd_status[conns] = -1;
          fd_method[conns] = -1;
          memset(fd_path[conns], 0, sizeof(fd_path[conns]));
          conns--;
          continue;
        }
        if (config->verbose) 
          printf("httpd: Request received from client:\n%s\n", request);

        memset(request_path, 0, sizeof(request_path));
        memset(request_method, 0, sizeof(request_method));
        status = parse_request(server, config, request, request_path, request_method);

        fd_method[i] = str_to_method(request_method);
        fd_status[i] = status;
        memset(fd_path[i], 0, sizeof(fd_path[i]));
        strcpy(fd_path[i], request_path);
      }

      // Message able to be sent to client;
      if (pfds[i].revents & POLLOUT && fd_path[i][0] != 0 && fd_status[i] != -1) {
        if (send_http(server, config, pfds[i].fd, fd_status[i], fd_path[i], fd_method[i]) != EXIT_SUCCESS)
          return EXIT_FAILURE;
        memset(fd_path[i], 0, sizeof(fd_path[i]));
        fd_status[i] = -1;
      } 
    }
  }
}

int server_free(struct server_t *server) {
  close(server->socket);
  server->socket = 0;
  return EXIT_SUCCESS;
}

