#include "csapp.h"

int main(int argc, char **argv)
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port> \n", argv[0]);
        exit(0);
    }
    // 입력한 값을 host와 port에 저장
    host = argv[1];
    port = argv[2];
    // Open_clientfd : 서버와 연결을 설정하는 함수
    // open_clientfd : 함수를 사용해서 소켓을 clientfd로 만듦
    clientfd = Open_clientfd(host, port);
    // clinetfd에 rio 주소를 연결
    Rio_readinitb(&rio, clientfd);
    
    // 표준 입력 스트림(eof)을 사용하여 입력이 없을때까지 반복
    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        // buf strlen(buf)만큼 복사해서 넣고
        Rio_writen(clientfd, buf, strlen(buf));
        // rio에 있는 값을 buf에 maxlen만큼 복사해서 넣음
        Rio_readlineb(&rio, buf, MAXLINE);
        // 표준출력으로 표시(buf를 stdout)
        Fputs(buf, stdout);
    }
    // 메모리 누수를 막기 위해 clientfd를 닫음
    Close(clientfd);
    exit(0);
}