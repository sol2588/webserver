/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.(GET메서드를 사용하여 정적 및 동적 콘텐츠를 제공하는 단순반복)
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include <stdlib.h>

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);



// do it :하나의 HTTP 요청/응답 트랜잭션 처리
/*
1. 클라이언트의 http 요청에서 요청라인만 읽음
2. get 메소드인지 확인
3. 요청 해더 들을 사용하지 않기 때문에 무시
4. uri를 잘라 uri, filename, cigiargs로 나누고 정적인지 동적인지 확인
6. 실행을 원하는 파일의 stat구조체의 st_mode를 이용해 파일이 읽기권한과 실행권한이 있는지 확인
*/
// fd는 connfd!, 클라이언트 요청 확인, 정정.동적 콘텐츠 확인
void doit(int fd)
{
    int is_static; // 정적 파일인지 아닌지 판단해주기 위한 변수
    struct stat sbuf; // 파일에 대한 정보를 가지고 있는 구조체
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE]; // 크기를 모르니까 max
    rio_t rio;

    // Read request line and headers : 요청 라인을 읽고 분석
    // 한개의 빈 버퍼를 설정하고, 이 버퍼와 한개의 오픈 파일 식별자를 연결
    Rio_readinitb(&rio, fd); // - Associate a descriptor with a read buffer and reset buffer(식별자 fd를 rio_t 타입의 읽기 버퍼(rio)와 연결하고 초기화)

    // 다음 텍스트 줄을 파일 rio에서 읽고, 이를 메모리 위치 buf로 복사후 텍스트 라인을 null
    if (!Rio_readlineb(&rio, buf, MAXLINE)) // 파일에서 텍스트 라인(readline)을 읽어 버퍼에 담는 함수 - Robustly read a text line (buffered)
        return;
    
    // 요청된 라인을 printf로 보여줌(최초 요청 라인: GET / HTTP/1.1)
    printf("%s", buf);

    // buf의 내용을 method, uri, version이라는 문장열에 저장
    sscanf(buf, "%s %s %s", method, uri, version); //sscanf : 버퍼에서 포맷을 지정하여 읽어오는 함수

    // GET, HEAD, 메소드만 지원(두 문자가 같으면 0)
    if (strcasecmp(method, "GET")) {
        // 다른 메소드가 들어올 경우, 에러를 출력하고 리턴
        clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
        
        return;
    }
    
    // 요청 라인을 제외한 요청 헤더를 출력
    read_requesthdrs(rio_t *rp); // request header를 읽는 함수

    // uri를 uri, filename, cgi argument string으로 parse하고, request가 정적인지 동적인지 확인하는 flag 리턴(1이면 정적)
    is_static = parse_uri(uri, filename, cgiargs); // 클라이언트가 요청한 uri를 파싱하는 함수
    
    // 디스크 파일이 없다면 filename을 sbuf에 넣음
    // 종류, 크기 등등이 sbuf에 저장 : 성공시 1, 실패시 -1
    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
        return;
    }


    // 서버 정적 콘텐츠
    if (is_static) {
        //S_ISREG(isregular->파일 종류 확인 : 일반 파일인지 판별
        //st_mode-> 파일의 유형값으로 bit& 연산으로 파일의 유형 확인 가능
        //읽기권한(S_IRUSR)을 가지고 있는지 판별
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            //읽기 권한이 없거나 정규 파일이 아니면 읽을 수 없음
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return;
        }
 
        serve_static(fd, filename, sbuf.st_size); // 정적 콘텐츠를 제공하는 함수
    }

    // 서버 동적 컨텐츠
    else {
        //실행권한(S_IXUSR)이 있는지 
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            // 정규파일이 아니거나 실행권한이 없으면 읽을 수 없음
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        
        // 실행 가능하면 동적 컨텐츠를 클라언트에게 제공
        serve_dynamic(fd, filename, cgiargs); // 동적 콘텐츠를 제공하는 함수
    }

}


// clienterror : client에게 HTTP응답을 보냄
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];
    //HTTP 응답 body
    //sprintf는 출력하는 결과 값을 변수에 저장하게 해주는 기능
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n",body);

    //Http 응답 출력
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}


/* read_requesthdrs
tiny는 request헤더의 내의 어떤 정보도 사용하지 않음 - 읽고 무시
요청 라인 한줄, 요청 헤더 여러줄을 받음
요청 라인은 저장(tiny에서 필요함), 요청헤더들은 그냥 출력
*/
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];
    // 한줄을 읽어들임('\0'을 만나면 종료)
    Rio_readlineb(rp, buf, MAXLINE);

    // strcmp(str1, str2) 같은 경우에 0 반환 -> 이경우에만 탈출
    // buf가 "\r\n"이 되는 경우는 모든 줄을 읽고 마지막 줄에 도착한 경우
    // 헤더의 마지막 줄은 비어있음
    while(strcmp(buf, "\r\n")) {
        // 한줄 한줄 읽은 것을 출력(최대 maxline까지 읽을 수 있음)
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}


/* parse_uri
uri를 parsing하여 static을 request하면 0, dynamic이면 1 반환

tiny는 정적 컨텐츠를 위한 홈 디렉토리가 자신의 현재 디렉토리이고,
실행파일의 홈 디렉토리는 /cgi-bin이라고 가정
*/
int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;
    /* 
    
     1. strstr(str1, str2) : 문자열 탐색 함수 - str1에서 str2와 일치하는 문자열이 있는지 확인하는 함수
        strstr(대상 문자열, 검색할 문자열)->검색할 문자열(대상문자열)뒤에 모든 문자열이 나옴
        uri에서 'cgi-bin'이라는 문자열이 없으면 정적 컨텐츠'(스트링 cgi-bin을 포함하는 모든 uri는 동적 컨텐츠)

     2. strcpy : 문자열 복사(대상 문자열 전체를 복사)
        strcpy(char *dest, cont char *src), dest - 복사를 받을 대상의 시작 주소, src - 복사할 원본의 시작 주소
     
     3. strcat : 문자열 연결 함수   strcat(char* dest, const char *origin) : origin에 있는 문자열을 dest뒤쪽에 이어붙이는 함수
    */

    // uri에 cgi-bin과 일치하는 문자가 없다면 cgiargs에는 빈문자열을 저장
    if (!strstr(uri, "cgi-bin")) {
        // cgiargs 인자 string을 지움
        strcpy(cgiargs, "");

        // 상대 리눅스 경로 이름으로 변환 - '.' + '/index.html'
        strcpy(filename, ".");
        strcat(filename, uri);

        // uri가 '/'문자로 끝나는 경우 기본파일 이름 추가
        if (uri[strlen(uri)-1] == '/')
            // filename에 home.html을 추가
            strcat(filename, "home.html");
        return 1;
    }

    // 동적 컨테츠
    else {
        // index 함수 : index(str1, str2) - str1에 str2가 있는지 찾아서 있으면 위치포인터, 없으면 NULL
        ptr = index(uri, '?');
        // 모든 cgi인자를 추출
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }

        // ?가 없는 경우
        else
            // 빈칸으로 둠
            strcpy(cgiargs, "");
        
        // 나머지 uri부분을 상대 리눅스 파일이름으로 변환
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}



/* server_static
서버의 디스크 파일을 정적 컨텐츠, 디스크 파일을 가져와 클라이언트에게 전달하는 작업을 정적 컨테츠
1. 컨텐츠를 보내기 전에 어떤 컨텐츠를 보낼지, 어느정도 크기의 컨텐츠를 보낼지를 포함한 response header 보내기
2. 요청한 파일을 읽기 전용으로 열고 파일의 내용을 가상메모리 영역에 저장
3. 가상 메모리에 저장된 내용을 클라이언트와 연결된 연결식별자에 작성해 컨텐츠를 클라이언트에게 보냄

파일을 클라이언트에 복사
tiny는 다섯개의 정적 컨텐츠 타입을 지원 : html, 무형식 텍스트파일, gif, png, jpeg
*/

// 서버가 디스크에서 파일을 찾아 메모리 영역으로 복사하고, 복한 것을 clientfd로 복사
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXLINE];

    // 5개중 무슨 파일 형식인지 검사해서 filetype을 채움
    get_filetype(filename, filetype);

    // client에 응답줄과 헤더를 보냄
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

    // buf에서 strlen(buf) 바이트만큼 fd로 전송
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);
    
    
    // open 함수(열려고 하는 대상 파일의 이름, 파일을 열때 적용되는 열기 옵션, 파일 열때의 접근 권한 설명)
    // return : 파일 디스크립터
    // O_RDONLY : 읽기 전용으로 파일 열기 -> filename을 읽기 전용으로 열어 식별자를 받음
    srcfd = Open(filename, O_RDONLY, 0);

    /*
    (요청한 파일을 가상 메모리 영역으로 매핑)
    mmap은 파일 srcfd의 첫번쨰 filesize 바이트를 주소 srcp에서 시작하는 읽기-허용 가상 메모리 영역으로 매핑
    mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
    말록과 비슷 : 값도 복사!
    */
   //PROT_READ - 읽기 가능 /  MAP_PRIVATE - 다른 프로세스와 대응 영역 공유하지 않음 
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    // 메모리로 매핑한 후에는 식별자 필요없어서 파일을 닫음
    Close(srcfd);
    // 실제로 파일을 client에 전송
    // Rio_writen 함수는 주소 srcp에서 시작하는 filesize를 클라이언트의 연결식발자 fd로 복사
    Rio_writen(fd, srcp, filesize);
    // 매핑된 가상 메모리 주소를 반환
    Munmap(srcp, filesize);
    
}

// get_filetype: 파일이름에서 파일 형식 가져오기
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4"); //11.7 mpg
    else 
        strcpy(filetype, "text/plain");
        
}


/* server_dynamic
1. 부모의 메모리를 물려 받는 자식 프로세스를 만들고 새로운 프로세스를 실행할 준비
2. exec는 fork와 달리 메모리를 물려받지 않기 때문에 전달하고 싶은 변수는 환경변수로 저장
 -> cgiargs를 QUERY_STRING 환경변수에 저장
3. 클라이언트와 연결된 connfd를 표준 출력으로 재설정, cgi프로그램이 표준 출력에 쓰는 모든 것은 직접 클라이언트 프로세스로 전달
4. 실행파일(cgi 프로그램)을 새로운 프로세스로 실행, 마지막 인자로 environ을 넣어주면 프로그램 내에서 getenv함수를 통해 기존에설정해던  QUERY_STRING변수를 사용할 수 있다
*/
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };
    // 클라이언트에 성공을 알려주는 응답 라인을 보내는 것으로 시작
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server : Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /*
    fork 함수를 호출하는 프로세스는 부모 프로세스, 새롭게 생성되는 프로세스는 자식 프로세스가 됨
    fork 함수에 의해 생성된 자식 프로세스는 부모 프로세스의 메모리를 그대로 복사함
    fork 함수 호출 이후 코드부터 각자의 메모리 사용해 실행
    */


    // 새로운 자식의 프로세스를 fork 함
    if (Fork() == 0) {
        // 이때 부모 프로세스는 자식의 pid(process id)를, 자식 프로세스는 0을 반환받음
        // query_string 환경변수를 요청 = uri의 cgi 인자들 넣겠다는 뜻
        // 세번째 인자는 기존 환경변수의 유무와 상관없이 값을 변경하겠다면 1, 아니면 0

        // setenv(const char* name, const char* value, int overwrite)
        // overwrite가 0이면 재설정 되지 않음, 그 외에는 주어진 값에 재설정됨
        // exec 호출하면 명령줄 인수, 환경변수만 전달받음
        setenv("QUERY_STRING", cgiargs, 1);
        // dup2 함수를 통해 표준 출력을 클라이언트와 연계된 연결 식별자로 재지정
        // -> cgi 프로그램이 표준 출력으로 쓰는 모든 것은 클라이언트로 바로감(부모 프로세스의 간선없이)
        Dup2(fd, STDOUT_FILENO);
        // cgi 프로그램을 실행- adder을 실행
        Execve(filename, emptylist, environ);
    } 

    // 자식이 아니면, 부모는 자식이 종료되는 것을 기다리기 위해 wait 함수에서 블록됨
    Wait(NULL);
}


// 클라이언트 : 브라우저
int main(int argc, char **argv)
{
    // listen & connected에 필요한 파일디스크립터
    int listenfd, connfd;
    // clinet host name, port를 저장할 배열 선언
    char hostname[MAXLINE], port[MAXLINE];
    // socklen_t : 소켓 관련 매개변수의 길이 및 크기 값에 대한 정의
    socklen_t clientlen; 
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    // 인자가 2개가 아닌 경우
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }


    // 듣기 소켓
    // argv는 main 함수가 받는 각각의 인자들
    // argv[1] : 우리가 부여하는 포트
    // 해당 포트에 연결 요청을 받을 준비가 된 듣기 식별자 리턴
    // 즉, 입력받은 port로 local에서 passive socket(연결 요청을 기다리는 소켓, 데이터 전송에 관여하지 않음) 생성
    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        clientlen = sizeof(clientaddr);
        // 서버는 accept 함수를 호출해서 클라이언트로부터의 연결 요청을 기다림
        // client 소켓은 server 소켓의 주소를 알고 있으니까 
        // client에서 서버로 넘어올때 addr 정보를 가지고 올 것이라고 가정
        // accept 함수는 연결되면 식별자를 리턴함
        // 듣기식별자, 소켓 주소 구조체의 주소, 주소(소켓 구조체)
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // line:netp:tiny:accept

        //clientaddr의 구조체에 대응되는 hostname, port 확인(clientaddr, clientlend 활용)
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        

        // 어떤 client가 들어왔는지 알려줌
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        
        //connfd로 트랜잭션 수행
        doit(connfd);  // line:netp:tiny:doit
        // coonfd로 자신쪽의 연결 끝 닫기
        Close(connfd); // line:netp:tiny:close
    }
}

