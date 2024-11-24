#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
// 假设有一个全局的错误日志文件指针
FILE *errorlog;

void write_errorlog(struct sockaddr_in addr, char *error) // 写入错误日志
{
    time_t curtime;
    time(&curtime);
    char *timenow = ctime(&curtime);
    fprintf(errorlog, "[%s][error][client %s %d]%s", timenow, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), error);
}
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h> // 包含 time() 函数所需的头文件

// 假设有一个全局的访问日志文件指针
FILE *accesslog;

typedef struct {
    char *http_method;
    char *http_uri;
    char *http_version;
} Request;

void write_accesslog(struct sockaddr_in addr, int code, Request *request) // 写入访问日志
{
    time_t curtime;
    time(&curtime);
    char *timenow = ctime(&curtime);
    if (request == NULL) // 请求解析失败
    {
        fprintf(accesslog, "%s--[%s]\"---\"%d", inet_ntoa(addr.sin_addr), timenow, code);
        return;
    }
    fprintf(accesslog, "%s--[%s]\"%s %s %s\"%d", inet_ntoa(addr.sin_addr), timenow, request->http_method, request->http_uri, request->http_version, code);
}

int main() {
    // 示例：打开错误日志文件
    FILE *p=fopen("abc","a");
    fprintf(p,"tju");
    errorlog = fopen("error.log", "a");
    if (errorlog == NULL) {
        perror("Error opening file");
        return 1;
    }
    fprintf(errorlog,"tju");
    // 示例：模拟一个错误
    struct sockaddr_in client_addr;
    client_addr.sin_addr.s_addr = inet_addr("192.168.1.1");
    client_addr.sin_port = htons(8080);

    char errorMessage[] = "Connection timeout";

    // 记录错误日志
    write_errorlog(client_addr, errorMessage);

    // 关闭错误日志文件
    fclose(errorlog);

    accesslog = fopen("access.log", "a");
    if (accesslog == NULL) {
        perror("Error opening file");
        return 1;
    }

    // 示例：模拟一个访问请求
    struct sockaddr_in client_addr1;
    client_addr1.sin_addr.s_addr = inet_addr("192.168.1.1");
    client_addr1.sin_port = htons(8080);

    Request req;
    req.http_method = "GET";
    req.http_uri = "/index.html";
    req.http_version = "HTTP/1.1";

    int responseCode = 200;

    // 记录访问日志
    write_accesslog(client_addr1, responseCode, &req);
    fprintf(accesslog,"tju");
    // 关闭访问日志文件
    fclose(accesslog);

    return 0;
}