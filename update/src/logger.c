#include "logger.h"

// 全局变量
FILE *accesslog;
FILE *errorlog;

void init_log()
{
    errorlog = fopen("errorlog.txt", "a"); // 以追加模式打开
    accesslog = fopen("accesslog.txt", "a");
    if (!errorlog || !accesslog)
        fprintf(stderr, "Error opening log file.\n");
}

void write_errorlog(struct sockaddr_in addr, char *error)
{
    time_t curtime;
    time(&curtime);
    char *timenow = ctime(&curtime);
    timenow[(int) strlen(timenow) - 1] = '\0'; // 去掉换行符
    int b = fprintf(errorlog, "[%s][error][client %s %d]%s",
                    timenow, inet_ntoa(addr.sin_addr),
                    ntohs(addr.sin_port), error);
    printf("%d", b);
    fflush(errorlog);
}

void write_accesslog(struct sockaddr_in addr, int code, Request *request)
{
    time_t curtime;
    time(&curtime);
    char *timenow = ctime(&curtime);
    timenow[(int) strlen(timenow) - 1] = '\0'; // 去掉换行符

    if (request == NULL) // 解析失败
    {
        int b = fprintf(accesslog, "%s -- [%s] \"---\" %d\n",
                        inet_ntoa(addr.sin_addr), timenow, code);
        printf("%d", b);
        fflush(accesslog);
        return;
    }

    fprintf(accesslog, "%s -- [%s] \"%s %s %s\" %d\n",
            inet_ntoa(addr.sin_addr), timenow,
            request->http_method, request->http_uri, request->http_version, code);
    fflush(accesslog);
}

void close_log()
{
    fclose(errorlog);
    fclose(accesslog);
}