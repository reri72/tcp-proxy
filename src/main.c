#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "net.h"
#include "proxy.h"
#include "event_loop.h"

int main(int argc, char *argv[])
{
    // 컴파일 경고 무시
    (void)argc;
    (void)argv;

    int server_fd = -1, epoll_fd = -1;

    if (setup_signals() == -1)
    {
        exit(EXIT_FAILURE);
    }

    if (set_socket(&server_fd) == -1)
    {
        if (server_fd >= 0)
            close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (set_epoll(&epoll_fd, server_fd) == -1)
    {
        close(server_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    printf("\033[36mlightweight proxy started {%d}\033[0m\n", PORT);

    int res = run_event_loop(epoll_fd, server_fd);

    close(server_fd);
    server_fd = -1;

    close(epoll_fd);
    epoll_fd = -1;

    printf("\033[36mserver closed\033[0m\n");

    return (res == 0) ? EXIT_SUCCESS : EXIT_FAILURE;

    return 0;
}