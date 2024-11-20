#include <stdio.h>
#include <parse.h>
#include <time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
char *get_filetype(char *filename);
void faliure_send(struct sockaddr_in addr, const int client_sock, const int sock, char *buf, int readret, int clear_buf);
void build_request(char *buf, char *filename, struct stat fbuf);
void reponse(struct sockaddr_in addr, int client_sock, int sock, Request *request, char *buf, int readret);
int close_socket(int sock);