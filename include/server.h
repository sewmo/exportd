#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#define HTTP_VERSION "HTTP/1.0"
#define STATUS_OK 200
#define STATUS_BAD_REQUEST 400
#define STATUS_FORBIDDEN 403
#define STATUS_NOT_FOUND 404
#define STATUS_NOT_IMPLEMENTED 501
#define STATUS_VERSION_NOT_SUPPORTED 505

#define PATH_LEN 128

enum method_t {
  HTTP_GET,
  HTTP_POST,
  HTTP_PUT
};

struct server_t {
  int socket;
  int port;
  struct sockaddr_in addr;
};

struct config_t {
  char rootdir[PATH_LEN/2];
  char access_log[PATH_LEN/2];
  char error_log[PATH_LEN/2];
  int max_conns;
  int max_queues;
  int port;
  bool verbose;
};

/* INFO: Supplies the fields of the specified config struct. */
void server_config(struct config_t *config);

/* INFO: Creates a listening socket for the http_server struct at port 80. Returns a non-zero value on failure. */
int server_init(struct server_t *server, struct config_t *config);

/* INFO: Blocks thread execution until a client connects. Returns a non-zero value on failure. */
int server_loop(struct server_t *server, struct config_t *config);

/* INFO: Closes the server, freeing its resources. Returns a non-zero value on failure. */
int server_free(struct server_t *server);

#endif
