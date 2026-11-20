#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BACKLOG     64
#define PORT        7728

int set_nonb(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl");
        return -1;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL");
        return -1;
    }

    return 0;
}

int set_socket(int *fd)
{
    *fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*fd < 0)
    {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        return -1;
    }

    if (set_nonb(*fd) == -1)
    {
        return -1;
    }

    struct sockaddr_in addr = {0};

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(*fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return -1;
    }

    if (listen(*fd, BACKLOG) < 0)
    {
        perror("listen");
        return -1;
    }

    return 0;
}

int set_epoll(int *epfd, int servfd)
{
    struct epoll_event ev = {0};

    *epfd = epoll_create1(0);
    if (*epfd < 0)
    {
        perror("epoll_create1");
        return -1;
    }

    ev.events = EPOLLIN | EPOLLET;  // 엣지트리거모드
    ev.data.fd = servfd;
    if (epoll_ctl(*epfd, EPOLL_CTL_ADD, servfd, &ev) < 0)
    {
        perror("epoll_ctl");
        return -1;
    }

    return 0;
}

#define MAX_EVENTS  64
int main(int argc, char *argv[])
{
    int server_fd = -1, epoll_fd = -1;

    if (set_socket(&server_fd) == -1)
    {
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (set_epoll(&epoll_fd, server_fd) == -1)
    {
        close(server_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    printf("lightweight proxy started {%d}\n", PORT);

    struct epoll_event events[MAX_EVENTS];
    int i = 0;
    while (i < 5)
    {
        // 할 일 있나 없나 무한 대기
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0)
        {
            if (errno == EINTR)
                continue;

            perror("epoll_wait");
            break;
        }

        i++;

        // 이벤트 발생 개수 순회
        for (int i = 0; i < nfds; i++)
        {
            // 리스닝 소켓
            if (events[i].data.fd == server_fd)
            {
                while (1)
                {
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    
                    // 새로운 client 받기
                    int cli_fd = accept(server_fd, (struct sockaddr *)&cli_addr, &cli_len);
                    if (cli_fd < 0)
                    {
                        // ET+논블로킹 => 쌓인 연결 모두 꺼내기
                        /*
                            EWOULDBLOCK 와 EAGAIN 는 사실상 같은 뜻 (다시 시도)
                            논블로킹이기 때문에 대기열이 비어 있을 때 accept를 호출하면 즉시 -1 리턴
                                => errno를 EAGAIN으로 설정
                        */

                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            // 대기 중인 모든 연결 수락 완료
                            break;
                        }
                        perror("accept");
                        break;
                    }

                    char cliip[INET_ADDRSTRLEN] = {0};
                    inet_ntop(AF_INET, &cli_addr.sin_addr, cliip, sizeof(cliip));
                    printf("client connected(fd:%d, ip: %s)\n", cli_fd, cliip);

                    const char *msg = "hello!\n";
                    write(cli_fd, msg, strlen(msg));
                    close(cli_fd);
                }
            }
            else
            {
                // 이미 연결된 소켓
            }
        }
    }

    close(server_fd);
    server_fd = -1;

    close(epoll_fd);
    epoll_fd = -1;

    return 0;
}