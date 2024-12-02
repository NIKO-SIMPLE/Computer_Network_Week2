#include "request.h"
#include "logger.h"

#define BUF_SIZE 8192

char *get_filetype(char *filename)
{
    if (strstr(filename, ".html"))
        return "text/html";
    else if (strstr(filename, ".gif"))
        return "image/gif";
    else if (strstr(filename, ".png"))
        return "image/png";
    else if (strstr(filename, ".css"))
        return "text/css";
    else if (strstr(filename, ".jpg") || strstr(filename, ".jpeg"))
        return "image/jpeg";
    else
        return "text/plain";
}
int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}
void faliure_send(struct sockaddr_in addr, const int client_sock, const int sock, char *buf, int readret, int clear_buf)
{
    if (send(client_sock, buf, readret, 0) != readret)
    {
        close_socket(client_sock);
        close_socket(sock);
        fprintf(stderr, "Error sending to client.\n");
        return;
    }
    if (clear_buf)
        memset(buf, 0, BUF_SIZE); // 更新缓冲区
    return;
}
void build_request(char *buf, char *filename, struct stat fbuf)
{
    memset(buf, 0, BUF_SIZE);           // 初始化缓冲区
    strcpy(buf, "HTTP/1.1 200 OK\r\n"); // 开始写入返回内容
    strcat(buf, "Connection: keep-alive\r\n");

    time_t curtime = time(NULL);
    char timenow[32];
    char filetime[32];
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

    // strcat(buf, get_filetype(filename)); // 添加文件类型
    strcat(buf, "Content-Type:");
    strcat(buf, get_filetype(filename));

    strcat(buf, "\r\n");
    strcat(buf, "\r\n"); // 补上回车
}
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
void response(struct sockaddr_in addr, int client_sock, int sock, Request *request, char *buf, int readret)
{
    if (request == NULL || readret > 8192)
    {
        char error400[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(client_sock, error400, strlen(error400), 0); // 解析失败，回显错误信息
        write_accesslog(addr, 400, request);
        memset(buf, 0, BUF_SIZE);
        return;
    }
    if (strcmp(request->http_version, "HTTP/1.1")) // 解析成功，版本不对
    {
        char error505[] = "HTTP/1.1 505 HTTP Version not supported\r\n\r\n";
        send(client_sock, error505, strlen(error505), 0); // 解析失败，回显错误信息
        write_accesslog(addr, 505, request);
        memset(buf, 0, BUF_SIZE);
        return;
    }
    if (strncmp(buf, "POST", 4) == 0) // 解析成功，格式符合要求
    {
        write_accesslog(addr, 200, request);
        send(client_sock, buf, readret, 0);
        memset(buf, 0, BUF_SIZE);
        return;
    }
    if(!strcmp(request->http_method, "HEAD") || !strcmp(request->http_method, "GET"))
    {
        readret = 0;
        struct stat fbuf;
        char filename[8192];
        memset(filename, 0, 8192);
        sprintf(filename, "static_site%s", request->http_uri); // 将格式化的字符串保存到filename
        if (strcmp(filename, "static_site/") == 0)
            strcat(filename, "index.html");
        if (stat(filename, &fbuf) == -1) //=0调用成功，否则失败，返回404
        {
            memset(buf, 0, BUF_SIZE);
            strcpy(buf, "HTTP/1.1 404 Not Found\r\n\r\n");
            readret = strlen(buf);
            faliure_send(addr, client_sock, sock, buf, readret, 1);
            write_accesslog(addr, 404, request);
            return;
        }
        build_request(buf, filename, fbuf);
        if (!strcmp(request->http_method, "HEAD")) // head直接返回buf
        {
            readret = strlen(buf);
            faliure_send(addr, client_sock, sock, buf, readret, 1);
            write_accesslog(addr, 200, request);
            return;
        }

        if (!strcmp(request->http_method, "GET"))
        {
            if (access(filename, R_OK) == -1) // 文件不可读或者不存在
            {
                memset(buf, 0, BUF_SIZE);
                strcpy(buf, "HTTP/1.1 403 Forbidden\r\n\r\n"); // 回显403
                readret = strlen(buf);
                faliure_send(addr, client_sock, sock, buf, readret, 1);
                write_accesslog(addr, 403, request);
                return;
            }
            FILE *file = fopen(filename, "rb"); // 打开文件，开始读
            if (file == NULL)
            {
                fprintf(stderr, "error open file\n");
                close_socket(sock);
                return;
            }
            int fsize = fbuf.st_size + strlen(buf); // 文件大小加头的大小
            char buffer[BUF_SIZE];
            if (fbuf.st_size + strlen(buf) <= BUF_SIZE) // 加起来没超buf大小
            {
                fread(buffer, sizeof(char), fbuf.st_size, file);
                readret = strlen(buf)+fbuf.st_size;
                memcpy(buf+strlen(buf), buffer,fbuf.st_size);
                faliure_send(addr, client_sock, sock, buf, readret, 0);
                write_accesslog(addr, 200, request);
                return;
            }

            if (fbuf.st_size + strlen(buf) > BUF_SIZE) // 超了，只装一个buf装得下的
            {
                int l =fread(buffer, sizeof(char), BUF_SIZE - strlen(buf), file);
                int a=strlen(buffer);
                memcpy(buf+strlen(buf), buffer,BUF_SIZE - strlen(buf));
                readret = BUF_SIZE;
                int b=strlen(buf);
                faliure_send(addr, client_sock, sock, buf, readret, 0);
                fsize = fsize - BUF_SIZE;
            }
            while (fsize > BUF_SIZE) // 多发几个buf
            {
                fread(buf, sizeof(char), BUF_SIZE, file);
                readret = BUF_SIZE;
                faliure_send(addr, client_sock, sock, buf, readret, 0);
                fsize = fsize - BUF_SIZE;
            }
            fread(buf, sizeof(char), fsize, file);
            readret = fsize;
            faliure_send(addr, client_sock, sock, buf, readret, 0);
            write_accesslog(addr, 200, request);
            return;
        }
    }
    char error501[] = "HTTP/1.1 501 Not Implemented\r\n\r\n";
    send(client_sock, error501, strlen(error501), 0);
    write_accesslog(addr, 501, request);
    return;
}