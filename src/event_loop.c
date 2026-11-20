#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "event_loop.h"
#include "proxy.h"

static volatile sig_atomic_t running = 1;

static void sighandle(int sig)
{
    (void)sig;
    running = 0;
}

int setup_signals(void)
{
    struct sigaction sa = {0};

    sa.sa_handler = sighandle;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) < 0)
    {
        perror("sigaction (SIGINT)");
        return -1;
    }

    if (sigaction(SIGTERM, &sa, NULL) < 0)
    {
        perror("sigaction (SIGTERM)");
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

int run_event_loop(int epfd, int servfd)
{
    struct epoll_event events[MAX_EVENTS];

    while (running)
    {
        // 할 일 있나 없나 무한 대기
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds < 0)
        {
            if (errno == EINTR)
                continue;

            perror("epoll_wait");
            return -1;
        }

        // 이벤트 발생 개수 순회
        for (int i = 0; i < nfds; i++)
        {
            // 리스닝 소켓
            if (events[i].data.fd == servfd)
            {
                while (1)
                {
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    
                    // 새로운 client 받기
                    int cli_fd = accept(servfd, (struct sockaddr *)&cli_addr, &cli_len);
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

    return 0;
}
