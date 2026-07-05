#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/server.h"
#include "../include/exporter.h"
#include "../include/daemonize.h"

/* NOTE: Exporter pipeline:
   1. HTTP/1.1 compliant web server is initialized at a known port.
   2. The exporter runs certain binaries, receives data from those binaries, parses it, formats it, then uploads the data to the files that the web server serves to clients.
   3. Example binaries: ps, lsblk, stat, lsusb;
   4. The database connects to the HTTP endpoint on the exporter's host, and receives the data.
*/

/* TODO:
   1. Use realpath() for path checking.
   2. Change process directory to web server root directory. 
   3. Finish making web server HTTP/1.1 compliant.
   4. After finishing web server, start working on the exporter. Use popen() for opening binary file descriptors.
*/

int main(int argc, char **argv) {
  /*
  if (daemonize(DA_NO_CHDIR) == -1) {
    perror("daemonize");
    return 2;
  }
  */

  struct server_t server;
  struct config_t config;

  server_config(&config);
  config.verbose = true;
  config.port = 9000;

  if (server_init(&server, &config) != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (server_loop(&server, &config) != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  if (server_free(&server) != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
