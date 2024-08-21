/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void sigchld_handler(int sig);

                 
void serve_static_head(int fd, char *filename, int filesize) {
    char filetype[MAXLINE], buf[MAXBUF];

    get_filetype(filename, filetype);
    snprintf(buf, MAXBUF, "HTTP/1.0 200 OK\r\n");
    snprintf(buf + strlen(buf), MAXBUF - strlen(buf), "Server: Tiny Web Server\r\n");
    snprintf(buf + strlen(buf), MAXBUF - strlen(buf), "Connection: close\r\n");
    snprintf(buf + strlen(buf), MAXBUF - strlen(buf), "Content-length: %d\r\n", filesize);
    snprintf(buf + strlen(buf), MAXBUF - strlen(buf), "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));

    printf("Response headers (HEAD request):\n");
    printf("%s", buf);
}

int main(int argc, char **argv) {
  Signal(SIGCHLD, sigchld_handler);
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers: \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  printf("Request line: %s", buf);
  read_requesthdrs(&rio);

  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }
  if (is_static)
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    if (strcasecmp(method, "HEAD") == 0)
    {
      serve_static_head(fd, filename, sbuf.st_size);
    } else{
      serve_static(fd, filename, sbuf.st_size);
    }
  }
  else{
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  } 
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  char buf[MAXLINE], body[MAXBUF];

  snprintf(body, MAXBUF, "<html><title>Tiny Error</title>");
  snprintf(body + strlen(body), MAXBUF - strlen(body), "<body bgcolor=\"ffffff\">\r\n");
  snprintf(body + strlen(body), MAXBUF - strlen(body), "%s: %s\r\n", errnum, shortmsg);
  snprintf(body + strlen(body), MAXBUF - strlen(body), "<p>%s: %s\r\n", longmsg, cause);
  snprintf(body + strlen(body), MAXBUF - strlen(body), "<hr><em>The Tiny Web server</em>\r\n");

  snprintf(buf, MAXLINE, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  snprintf(buf, MAXLINE, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  snprintf(buf, MAXLINE, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}


int parse_uri(char *uri, char *filename, char *cgiargs){
  char *ptr;

  if (!strstr(uri, "cgi-bin")){
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/'){
      strcat(filename, "home.html");
    return 1;
    }
  }
  else{
    ptr = strchr(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  get_filetype(filename, filetype);
  snprintf(buf, MAXBUF, "HTTP/1.0 200 OK\r\n");
  snprintf(buf + strlen(buf), MAXBUF - strlen(buf), "Server: Tiny Web Server\r\n");
  snprintf(buf + strlen(buf), MAXBUF - strlen(buf), "Connection: close\r\n");
  snprintf(buf + strlen(buf), MAXBUF - strlen(buf), "Content-length: %d\r\n", filesize);
  snprintf(buf + strlen(buf), MAXBUF - strlen(buf), "Content-type: %s\r\n\r\n", filetype);
  Rio_writen(fd, buf, strlen(buf));

  printf("Response headers:\n");
  printf("%s", buf);

  
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  free(srcp);
}


void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html"))
  {
    strcpy(filetype, "text/html");
  } else if (strstr(filename, ".gif"))
  {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".png"))
  {
    strcpy(filetype, "image/png");
  } else if (strstr(filename, ".jpg"))
  {
    strcpy(filetype, "image/jpeg");
  }else if (strstr(filename, ".mpg"))
  {
    strcpy(filetype, "video/mpeg");
  }
   else{
    strcpy(filetype, "text/plain");
  }
}

void serve_dynamic(int fd, char *filename, char *cgiargs){
  char buf[MAXLINE], *emptylist[] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0)
  {
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);
    Execve(filename, emptylist, environ);
  }
  Wait(NULL);
}

void sigchld_handler(int sig){
  while (waitpid(-1, NULL, WNOHANG) > 0){

  }
}