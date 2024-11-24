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
/*#include <netinet/in.h>

struct sockaddr_in {
    sa_family_t    sin_family;   // 地址族 (AF_INET)
    in_port_t      sin_port;     // 端口号 (使用 htons() 转换)
    struct in_addr sin_addr;     // IP 地址
    char           sin_zero[8];  // 填充使结构大小与 sockaddr 一致
};
sin_family: 必须设置为 AF_INET。
sin_port: 保存端口号，必须使用 htons() 函数将主机字节序转换为网络字节序。
sin_addr: 保存 IP 地址，可以使用 inet_pton() 或 inet_aton() 函数设置。
sin_zero: 仅用于填充，不做其他用途。
*/

/*日志格式为"[%t] [%l] [pid %P] [client %a] %m: %M"
(其中%t 为当前时间，%1 为消息的日志级别，%P
为当前进程的进程 ID，%a 为客户端 IP 地址和请求的端口，%m 为记录消息的模
块名称，%M 为实际的日志消息)，典型的日志信息如"[Tue Nov 29 03:23:06 2022]
[error] [lpid 19473] [client 127.0.0.1:49064] socket: Connection reset by peer"所示。*/
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <parse.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>
#define ECHO_PORT 9999
#define BUF_SIZE 8192

FILE *accesslog;
FILE *errorlog;
struct sockaddr_in addr, cli_addr;


void write_errorlog(struct sockaddr_in addr, char* error){
    time_t curtime;
    time(&curtime);
    char *timenow=ctime(&curtime);
    int b=fprintf(errorlog, "[%s][error][client %s %d]%s",timenow, inet_ntoa(addr.sin_addr),ntohs(addr.sin_port),error);
    printf("%d",b);
    fflush(errorlog);
}
/*[%s]：记录当前时间。
    [error]：标记为错误日志。
    [client %s %d]：记录客户端的 IP 地址和端口号。
    %s：记录具体的错误信息*/

void write_accesslog(struct sockaddr_in addr, int code, Request *request) // 写入正确日志
{

    time_t curtime;
    time(&curtime);
    char *timenow = ctime(&curtime);
    if (request == NULL) // 解析失败
    {
        int a=fprintf(accesslog, "%s--[%s]\"---\"%d", inet_ntoa(addr.sin_addr), timenow, code);
        printf("%d",a);
        fflush(accesslog);
        return;
    }
    fprintf(accesslog, "%s--[%s]\"%s %s %s\"%d",inet_ntoa(addr.sin_addr), timenow, request->http_method, request->http_uri, request->http_version, code);
    fflush(accesslog);
}
int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        write_errorlog(addr, strerror(errno)); // 关闭失败，写入错误日志，strerror 函数用于返回对应于错误码的错误信息字符串。
        return 1;
    }
    return 0;
}
void init_log()
{
    errorlog = fopen("errorlog.txt", "a"); // 以只写形式打开
    accesslog = fopen("accesslog.txt", "a");
    if (!errorlog || !accesslog)
    {
        fprintf(stderr, "Error opening log file.\n"); // 开不了就报错
        return ;
    }
    return ;
}
void get_filetype(char *filename, char *filetype) // 检查文件格式
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".css"))
        strcpy(filetype, "text/css");
    else if (strstr(filename, ".jpg") || strstr(filename, ".jpeg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}
int send_message(int client_socket, char *buf, int length)
{
    int readret;
    if ((readret = send(client_socket, buf, length, 0)) != length)
    {
        close_socket(client_socket);
        fprintf(stderr, "Error sending to client.\n");
        write_errorlog(addr, strerror(errno));
        return -1;
    }
    return readret;
}

void faliure_send(const int client_sock, const int sock, char *buf, int readret, int clear_buf)
{
    if (send(client_sock, buf, readret, 0) != readret)
    {
        close_socket(client_sock);
        close_socket(sock);
        fprintf(stderr, "Error sending to client.\n");
        write_errorlog(addr, strerror(errno));
        return;
    }
    if (clear_buf)
        memset(buf, 0, BUF_SIZE); // 更新缓冲区
    return;
}

void response(char *buf, int readret, Request *a, int sock, int client_sock)
{
    struct stat fbuf;
    /*struct stat 包含以下常见字段：
    st_mode：文件类型和权限。
    st_ino：inode 号。
    st_dev：设备 ID。
    st_nlink：硬链接数。
    st_uid：所有者的用户 ID。
    st_gid：所有者的组 ID。
    st_size：文件大小（字节）。
    st_atime：最后访问时间。
    st_mtime：最后修改时间。
    st_ctime：最后状态更改时间。*/
    char filename[8192];
    char filetype[32];
    char filetime[32];
    memset(filename, 0, 8192);
    sprintf(filename, "static_site%s", a->http_uri); // 将格式化的字符串保存到filename
    if (strcmp(filename, "static_site/") == 0)
        strcat(filename, "index.html");
    if (stat(filename, &fbuf) == -1)                 //=0调用成功，否则失败，返回404
    {
        memset(buf, 0, BUF_SIZE);
        strcpy(buf, "HTTP/1.1 404 Not Found\r\n\r\n");
        readret = strlen(buf);
        write_accesslog(addr, 404, a); // 写入错误日志
        faliure_send(client_sock, sock, buf, readret, 1);
        return;
    }
    if (stat(filename, &fbuf) == -1) //=0调用成功，否则失败，返回404
    {
        memset(buf, 0, BUF_SIZE);
        strcpy(buf, "HTTP/1.1 404 Not Found\r\n\r\n");
        readret = strlen(buf); // 写入连接日志
        write_accesslog(addr, 404, a);
        faliure_send(client_sock, sock, buf, readret, 1);
        return;
    }
    get_filetype(filename, filetype);   // 获取文件类型
    memset(buf, 0, BUF_SIZE);           // 初始化缓冲区
    strcpy(buf, "HTTP/1.1 200 OK\r\n"); // 开始写入返回内容
    strcat(buf, "Connection: keep-alive\r\n");

    time_t curtime = time(NULL);
    char timenow[32];
    char temp[64];
    strftime(timenow, sizeof(timenow), "%a, %d %b %Y %H:%M:%S %Z", gmtime(&curtime)); // gmtime 函数用于将时间转换为协调世界时（UTC）的结构体格式。
    sprintf(temp, "Date: %s\r\n", timenow);
    strcat(buf, temp); // 处理时间

    strcat(buf, "Server: liso/1.0\r\n");

    strftime(filetime, sizeof(timenow), "%a, %d %b %Y %H:%M:%S %Z", gmtime(&(fbuf.st_mtime)));
    sprintf(temp, "Last-Modified: %s\r\n", filetime);
    strcat(buf, temp); // 添加最后修改时间

    sprintf(temp, "Content-Length: %ld\r\n", fbuf.st_size);
    strcat(buf, temp); // 添加大小

    sprintf(temp, "Content-Type: %s\r\n", filetype);
    strcat(buf, temp); // 添加文件类型

    strcat(buf, "\r\n");                 // 补上回车
    if (!strcmp(a->http_method, "HEAD")) // head直接返回buf
    {
        readret = strlen(buf);
        write_accesslog(addr, 200, a);
        faliure_send(client_sock, sock, buf, readret, 1);
    }
    if (!strcmp(a->http_method, "GET"))
    {
        if (access(filename, R_OK) == -1) // 文件不可读或者不存在
        {
            memset(buf, 0, BUF_SIZE);
            strcpy(buf, "HTTP/1.1 403 Forbidden\r\n\r\n"); // 回显403
            readret = strlen(buf);
            write_accesslog(addr, 403, a);
            faliure_send(client_sock, sock, buf, readret, 1);
            return;
        }
        FILE *file = fopen(filename, "rb"); // 打开文件，开始读
        if (file == NULL)
        {
            fprintf(stderr, "error open file\n");
            write_errorlog(addr, strerror(errno));
            close_socket(sock);
            return;
        }
        write_accesslog(addr, 200, a);
        int fsize = fbuf.st_size + strlen(buf); // 文件大小加头的大小
        char buffer[BUF_SIZE];
        if (fbuf.st_size + strlen(buf) <= BUF_SIZE) // 加起来没超buf大小
        {
            fread(buffer, sizeof(char), fbuf.st_size, file);
            strcat(buf, buffer);
            readret = strlen(buf);
            faliure_send(client_sock, sock, buf, readret, 0);
            return;
        }
        if (fbuf.st_size + strlen(buf) > BUF_SIZE) // 超了，只装一个buf装得下的
        {
            fread(buffer, sizeof(char), BUF_SIZE - strlen(buf), file);
            strcat(buf, buffer);
            readret = BUF_SIZE;
            faliure_send(client_sock, sock, buf, readret, 0);
            fsize = fsize - BUF_SIZE;
        }
        while (fsize > BUF_SIZE) // 多发几个buf
        {
            fread(buf, sizeof(char), BUF_SIZE, file);
            readret = BUF_SIZE;
            faliure_send(client_sock, sock, buf, readret, 0);
            fsize = fsize - BUF_SIZE;
        }
        fread(buf, sizeof(char), fsize, file);
        readret = strlen(buf);
        faliure_send(client_sock, sock, buf, readret, 0);

    }
}

int main(int argc, char *argv[])
{
    int sock, client_sock;
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    char buf[BUF_SIZE];

    fprintf(stdout, "----- liso Server -----\n");

    init_log();
    /*void init_log()
    {
        errorlog = fopen("errorlog.txt", "a"); // 以只写形式打开
        accesslog = fopen("accesslog.txt", "a");
        if (!errorlog || !accesslog)
        {
            fprintf(stderr, "Error opening log file.\n"); // 开不了就报错
            return ;
        }
        return ;
    }*/
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
        if ((client_sock = accept(sock, (struct sockaddr *)&cli_addr,&cli_size)) == -1)
        {
            close(sock);
            fprintf(stderr, "Error accepting connection.\n");
            write_errorlog(addr, strerror(errno));
            return EXIT_FAILURE;
        }

        readret = 0;
        memset(buf, 0, BUF_SIZE);
        while ((readret = recv(client_sock, buf, BUF_SIZE, 0)) >= 1)
        {

            Request *request = parse(buf, readret);
            if (request == NULL || readret > 8192)
            {
                char error400[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
                send(client_sock, error400, strlen(error400), 0); // 解析失败，回显错误信息
                write_accesslog(addr,400,request);
                memset(buf, 0, BUF_SIZE);
            }
            else if (strcmp(request->http_version,"HTTP/1.1")) // 解析成功，版本不对
            {
                char error505[] = "HTTP/1.1 505 HTTP Version not supported\r\n\r\n";
                send(client_sock, error505, strlen(error505), 0); // 解析失败，回显错误信息
                write_accesslog(addr,505,request);
                memset(buf, 0, BUF_SIZE);
            }
            else if (strncmp(buf, "POST", 4) == 0) // 解析成功，格式符合要求
            {
                write_accesslog(addr, 200, request);
                send(client_sock, buf, readret, 0);
                memset(buf, 0, BUF_SIZE);
            }
            else if (strncmp(buf, "GET", 3) == 0 || strncmp(buf, "HEAD", 4) == 0) // 解析成功，格式符合要求
            {
                response(buf, readret, request, sock, client_sock);
                memset(buf, 0, BUF_SIZE);
            }
            else // 解析成功，但不符合格式要求
            {
                char error501[] = "HTTP/1.1 501 Not Implemented\r\n\r\n";
                send(client_sock, error501, strlen(error501), 0);
                write_accesslog(addr, 501, request);
            }
            memset(buf, 0, BUF_SIZE);
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
    fclose(errorlog);
    fclose(accesslog);
    close_socket(sock);
    return EXIT_SUCCESS;
}
