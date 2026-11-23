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
#include "net.h"
#include "proxy.h"

static volatile sig_atomic_t running = 1;

static int peer_fds[MAX_FD];

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

    signal(SIGPIPE, SIG_IGN);       // write 에러 방어

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

    ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;  // 엣지트리거모드
    ev.data.fd = servfd;
    if (epoll_ctl(*epfd, EPOLL_CTL_ADD, servfd, &ev) < 0)
    {
        perror("server epoll_ctl");
        return -1;
    }

    return 0;
}

static int add_epoll_fd(int epfd, int fd)
{
    struct epoll_event ev = {0};
    ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
    ev.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
    {
        perror("epoll_ctl ADD");
        return -1;
    }
    return 0;
}

static void close_pair(int epfd, int fd1, int fd2)
{
    if (fd1 >= 0 && fd1 < MAX_FD)
    {
        peer_fds[fd1] = -1;
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd1, NULL);
        close(fd1);
    }

    if (fd2 >= 0 && fd2 < MAX_FD)
    {
        peer_fds[fd2] = -1;
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd2, NULL);
        close(fd2);
    }
}

int run_event_loop(int epfd, int servfd)
{
    struct epoll_event events[MAX_EVENTS];

    // 초기화
    for (int i = 0; i < MAX_FD; i++)
        peer_fds[i] = -1;

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
            int fd = events[i].data.fd;
            uint32_t ev_flags = events[i].events;

            // 리스닝 소켓
            if (fd == servfd)
            {
                if (ev_flags & (EPOLLERR | EPOLLHUP))
                {
                    fprintf(stderr, "server error (fd:%d)\n", servfd);
                    return -1;
                }

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

                    if (cli_fd >= MAX_FD)
                    {
                        fprintf(stderr, "client (%d) exceeds MAX_FD \n", cli_fd);
                        close(cli_fd);
                        continue;
                    }

                    if (set_nonb(cli_fd) < 0)
                    {
                         close(cli_fd);
                         continue;
                    }

                    int backend_fd = connect_backend(TARGET_IP, TARGET_PORT);
                    if (backend_fd < 0)
                    {
                        close(cli_fd);
                        continue;
                    }

                    if (backend_fd >= MAX_FD)
                    {
                        fprintf(stderr, "backend (%d) exceeds MAX_FD \n", backend_fd);
                        close(cli_fd);
                        close(backend_fd);
                        continue;
                    }

                    if (add_epoll_fd(epfd, cli_fd) < 0 || add_epoll_fd(epfd, backend_fd) < 0)
                    {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, cli_fd, NULL);
                        close(cli_fd);
                        close(backend_fd);
                        continue;
                    }

                    peer_fds[cli_fd] = backend_fd;
                    peer_fds[backend_fd] = cli_fd;

                    char cliip[INET_ADDRSTRLEN] = {0};
                    inet_ntop(AF_INET, &cli_addr.sin_addr, cliip, sizeof(cliip));
                    printf("peer connected: client fd=%d (%s) <-> backend fd=%d (%s:%d)\n",
                                        cli_fd, cliip, backend_fd, TARGET_IP, TARGET_PORT);
                }
            }
            else
            {
                if (fd < 0 || fd >= MAX_FD || peer_fds[fd] < 0)
                    continue;

                int target_fd = peer_fds[fd];

                if (ev_flags & (EPOLLERR | EPOLLHUP))
                {
                    printf("socket error: fd=%d, peer=%d\n", fd, target_fd);
                    close_pair(epfd, fd, target_fd);
                    continue;
                }

                char buf[BUF_SIZE] = {0};
                int closed = 0;

                while (1)
                {
                    ssize_t n = read(fd, buf, sizeof(buf));
                    if (n > 0)
                    {
                        ssize_t totlen = 0;
                        while (totlen < n)
                        {
                            ssize_t sent = write(target_fd, buf + totlen, (size_t)(n - totlen));
                            if (sent < 0)
                            {
                                if (errno == EINTR)
                                    continue;
                                if (errno == EAGAIN || errno == EWOULDBLOCK)
                                {
                                    // 송신 버퍼가 일시적으로 꽉 찬 경우
                                    fprintf(stderr, "write error (target_fd:%d, from fd:%d)\n", target_fd, fd);
                                    closed = 1;
                                    break;
                                }

                                perror("write");
                                closed = 1;
                                break;
                            }
                            totlen += sent;
                        }
                        
                        if (closed)
                            break;

                        printf("relay : %.*s(%zd)\n", (int)totlen, buf, totlen);
                    }
                    else if (n == 0)
                    {
                        printf("connection closed: fd=%d (peer=%d)\n", fd, target_fd);
                        closed = 1;
                        break;
                    }
                    else
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        if (errno == EINTR)
                            continue;

                        perror("read");
                        closed = 1;
                        break;
                    }
                }

                if (closed)
                {
                    close_pair(epfd, fd, target_fd);
                }
            }
        }
    }

    return 0;
}
