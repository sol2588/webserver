#include "csapp.h"

/*
- 서버는 한 순간에 하나의 클라이언트와 연결되어 에코 서비스를 제공
- 서버는 입력된 개수의 클라이언트에게 순차적으로 서버를 제공하고 종료
- 클라이언트는 프로그램 사용자로부터 문자열 데이터를 입력받아서 서버에 전송
- 서버는 전송 받은 문자열 데이터를 클라이언트에게 재전송(에코)
- 서버와 클라이언트간의 문자열 에코는 클라이언트가 Q를 입력할때까지 진행
*/


void echo(int connfd);

int main(int argc, char **argv) // argc : 실행시 지정해준 '명령 옵션의 개수', argv : '명령 옵션의 문자열' 실제 저장되는 배열
                                // argc는 항상 1, 하나도 입력하지 않았을때 1이고, 0이 되지 않음
{
    int listenfd, connfd;
    socklen_t clientlen; // socklen_t : 소켓 관련 매개변수의 길이 및 크기 값에 대한 정의
    struct sockaddr_storage clientaddr; // accept와 연결되는 소켓 주소 구조체
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2){ // argv의 개수가 2개가 아니면 error
        fprintf(stderr, "usage : %s <port>\n", argv[0]);
        exit(0); // 0이 아니면 정상적인 종료가 아님
    }

    // open_listenfd : 대기 상태인 듣기 식별자를 생성하는 함수(port를 인자로 받음)
    // server 소켓을 listenfd로 만들어서 대기
    listenfd = Open_listenfd(argv[1]);

    // eof(end of file)가 들어오기 전까지 계속 돌음
    while (1) {
        // clientlen은 sockaddr_storage 구조체의 크기
        clientlen = sizeof(struct sockaddr_storage);
        // listenfd가 구조체 clientaddr을 만나면 accept가 되고, 새로운 연결 식별자를 connfd에 return
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        // clientaddr, clientlen을 이용해, client_hostname, client_port를 만듦
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);

        // client_hostname, client_port 출력
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        
        // echo 함수에 connfd를 넣어서 돌림
        // 여기서 buf에 connfd를 복사함
        echo(connfd);
        // 닫음
        Close(connfd);
    }
    exit(0);
}