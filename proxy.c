#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void handle_client(int connfd);  // 요청 처리 함수
void forward_request(int connfd, char *host, char *path, char *port); // 요청 전달 함수
void read_request_headers(rio_t *rp);  // 요청 헤더 읽는 함수
int parse_uri(char *uri, char *host, char *path, char *port);  // URI 파싱 함수

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) // 인자 2개 확인
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);  // 소켓 열기

  while (1)   // 클라이언트 요청 지속 처리
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // 클라이언트 수락
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port,MAXLINE, 0); // 클라이언트 정보
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    handle_client(connfd); // 클라이언트 요청 처리

    Close(connfd);
  }
}

void handle_client(int connfd) {
    rio_t rio;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], path[MAXLINE], port[MAXLINE];

    // 클라이언트의 요청 분석
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    // GET 메서드만 처리
    if (strcasecmp(method, "GET")) {
        printf("Proxy does not implement the method: %s\n", method);
        return;
    }

    // URI 파싱
    if (parse_uri(uri, host, path, port) < 0) {
        printf("URI parsing error.\n");
        return;
    }

    // 원 서버에 요청
    forward_request(connfd, host, path, port);
}

void read_request_headers(rio_t *rp){
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

void forward_request(int connfd, char *host, char *path, char *port) {
    int serverfd;
    char buf[MAXLINE], request[MAXLINE];
    rio_t server_rio;

    // 원 서버에 연결
    serverfd = Open_clientfd(host, port);
    if (serverfd < 0) {
        printf("Failed to connect to server (%s:%s): %s\n", host, port, strerror(errno));
        return;
    }

    // HTTP 요청
    sprintf(request, "GET %s HTTP/1.0\r\n", path);
    sprintf(request + strlen(request), "Host: %s:%s\r\n", host, port);
    strcat(request, user_agent_hdr);
    strcat(request, "Connection: close\r\n");
    strcat(request, "Proxy-Connection: close\r\n\r\n");

    // 원 서버에 전송
    Rio_writen(serverfd, request, strlen(request));

    // 클라이언트에 전달
    Rio_readinitb(&server_rio, serverfd);
    size_t n;
    while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
        printf("Forwarding %zu bytes to client\n", n);
        Rio_writen(connfd, buf, n);
    }

    Close(serverfd);
}

int parse_uri(char *uri, char *host, char *path, char *port) {
    char *host_begin, *host_end, *path_begin;

    // URI가 "http://"로 시작하는지 확인
    if (strncasecmp(uri, "http://", 7) != 0) {
        return -1; 
    }

    // "http://" 다음부터 호스트 정보 시작
    host_begin = uri + 7;

    // 호스트 끝 위치 ("/" 또는 ":")
    host_end = strpbrk(host_begin, "/:");

    // 호스트 복사
    if (host_end != NULL) {
        strncpy(host, host_begin, host_end - host_begin);
        host[host_end - host_begin] = '\0';  // 문자열 끝
    } else {
        strcpy(host, host_begin);  // 경로가 없을 때는 전체를 호스트로
    }

    // 포트가 있을 경우
    if (*host_end == ':') {
        host_end++;
        sscanf(host_end, "%[^/]", port);  // "/" 전까지 포트를 파싱
    } else {
        strcpy(port, "80");  // 기본 포트는 80
    }

    // 경로 찾기
    path_begin = strchr(host_end, '/');
    if (path_begin != NULL) {
        strcpy(path, path_begin);  // 경로 정보 복사
    } else {
        strcpy(path, "/");  // 경로가 없으면 "/"로 설정
    }

    return 0; 
}
