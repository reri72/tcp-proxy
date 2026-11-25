#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include "net.h"
#include "proxy.h"
#include "event_loop.h"

static void print_usage(void)
{
    fprintf(stderr,  "Usage:\n"
                    "[-l listen_port] \n"
                    "[-h]\n");
}

static int parse_args(int argc, char *argv[],
                        int *listen_port)
{
    int opt;
    while ((opt = getopt(argc, argv, "l:h")) != -1)
    {
        switch (opt)
        {
            case 'l':
                *listen_port = strtol(optarg, NULL, 10);
                break;

            case 'h':
                print_usage();
                exit(0);

            default:
                print_usage();
                return -1;
        }
    }

    if (*listen_port <= 0)
    {
        *listen_port = PORT;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int listen_port = 0;
    int server_fd = -1, epoll_fd = -1;

    if (parse_args(argc, argv, &listen_port) == -1)
        return -1;

    if (setup_signals() == -1)
    {
        exit(EXIT_FAILURE);
    }

    if (set_socket(&server_fd, listen_port) == -1)
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

    printf("\033[36mlightweight proxy started {%d}\033[0m\n", listen_port);

    int res = run_event_loop(epoll_fd, server_fd);

    close(server_fd);
    server_fd = -1;

    close(epoll_fd);
    epoll_fd = -1;

    printf("\033[36mserver closed\033[0m\n");

    return (res == 0) ? EXIT_SUCCESS : EXIT_FAILURE;

    return 0;
}