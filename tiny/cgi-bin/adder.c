/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  if ((buf = getenv("QUERY_STRING")) != NULL) {
    // n1 파싱
    p = strchr(buf, '=');
    if (p != NULL) {
      *p = '\0';  // '=' 문자 제거
      strcpy(arg1, p + 1);  // n1 값 추출
      p = strchr(arg1, '&');  // '&' 문자를 찾아 n1과 n2 구분
      if (p != NULL) {
        *p = '\0';  // '&' 문자 제거
        strcpy(arg2, p + 4);  // n2 값 추출
        n1 = atoi(arg1);  // n1을 정수로 변환
        n2 = atoi(arg2);  // n2을 정수로 변환
      }
    }
  }

  snprintf(content, MAXLINE, "Welcome to add.com: ");
  snprintf(content + strlen(content), MAXLINE - strlen(content), "THE internet addition portal.\r\n<p>");
  snprintf(content + strlen(content), MAXLINE - strlen(content), "The answer is: %d + %d = %d\r\n<p>", n1, n2, n1 + n2);
  snprintf(content + strlen(content), MAXLINE - strlen(content), "Thanks for visiting!\r\n");

  // HTTP 헤더 출력
  printf("Connection: close\r\n");

  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */
