#ifndef LOGGER_H
#define LOGGER_H

#include "parse.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void init_log();
void write_errorlog(struct sockaddr_in addr, char *error);
void write_accesslog(struct sockaddr_in addr, int code, Request *request);
void close_log();

#endif // LOGGER_H
