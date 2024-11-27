/******************************************************************************
 * echo_server.c                                                               *
 *                                                                             *
 * Description: This file contains the C source code for an echo server.  The  *
 *              server runs on a hard-coded port and simply write back anything*
 *              sent to it by connected clients.  It does not support          *
 *              concurrent clients.                                            *
 *                                                                             *
 * Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
 *          Wolf Richter <wolf@cs.cmu.edu>                                     *
 *                                                                             *
 *******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>

#include "request.h"
#include "logger.h"
#define ECHO_PORT 9999
#define BUF_SIZE 8192

struct sockaddr_in addr, cli_addr;

int spilt(char *buf, char **list)
{
    int readret = strlen(buf);
    int count=0;
    char *p = buf;
    char *q = buf;
    int length = 0;
    if (buf == NULL || readret == 0)
    {
        return -1;
    }
    while (*p != '\0')
    {
        if (*p == '\r')
        {
            if (!strncmp(p, "\r\n\r\n", 4))
            {
                length = p - q;
                list[count] = (char *)malloc(length + 5);
                strncpy(list[count], q, length);
                (list[count])[length] = '\r';
                (list[count])[length + 1] = '\n';
                (list[count])[length + 2] = '\r';
                (list[count])[length + 3] = '\n';
                (list[count])[length + 4] = '\0';
                count++;
                p = p + 4;
                q += length + 4;
                continue;
            }
        }
        p++;
    }
    return count;
}

int main(int argc, char *argv[])
{
    int sock, client_sock;
    ssize_t readret;
    socklen_t cli_size;
    char buf[BUF_SIZE];

    fprintf(stdout, "----- Echo Server -----\n");
    init_log();
    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        write_errorlog(addr, strerror(errno));
        return EXIT_FAILURE;
    }

    if (listen(sock, 5))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        write_errorlog(addr, strerror(errno));
        return EXIT_FAILURE;
    }

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        cli_size = sizeof(cli_addr);
        if ((client_sock = accept(sock, (struct sockaddr *)&cli_addr,
                                  &cli_size)) == -1)
        {
            close(sock);
            fprintf(stderr, "Error accepting connection.\n");
            write_errorlog(addr, strerror(errno));
            return EXIT_FAILURE;
        }

        readret = 0;

        while ((readret = recv(client_sock, buf, BUF_SIZE, 0)) >= 1)
        {
            int count = 0;
            Request *request;
            char *list[100] = {0};
            count = spilt(buf, list);
            int i;
            for (i = 0; i < count; i++)
            {
                memset(buf, 0, BUF_SIZE);
                strcpy(buf, list[i]);
                request = parse(buf, BUF_SIZE);
                reponse(addr,client_sock,sock,request,buf,strlen(buf));
                memset(buf, 0, BUF_SIZE);
            }
        }

        if (readret == -1)
        {
            close_socket(client_sock);
            close_socket(sock);
            fprintf(stderr, "Error reading from client socket.\n");
            write_errorlog(addr, strerror(errno));
            return EXIT_FAILURE;
        }

        if (close_socket(client_sock))
        {
            close_socket(sock);
            fprintf(stderr, "Error closing client socket.\n");
            write_errorlog(addr, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    close_socket(sock);
    close_log();
    return EXIT_SUCCESS;
}