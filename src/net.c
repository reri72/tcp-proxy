#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "proxy.h"
#include "net.h"

/**
 * @brief socket fd를 nonblocking 으로 설정
 * 
 * @param socket fd
 * @return 성공 시 0, 실패 시 -1
 */
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

/**
 * @brief 프록시 서버 소켓 생성 및 설정
 * 
 * @param empty socket fd
 * @return 성공 시 리슨 상태의 fd, 실패 시 -1
 */
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

/**
 * @brief peer 서버 연결 소켓 생성
 * 
 * @param peer ip 
 * @param peer port 
 * @return 성공 시 peer 서버 연결 fd, 실패 시 -1
 */
int connect_backend(const char *ip, int port)
{
    int backend_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (backend_fd < 0)
    {
        perror("backend socket");
        return -1;
    }

    struct sockaddr_in backend_addr = {0};
    backend_addr.sin_family = AF_INET;
    backend_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &backend_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(backend_fd);
        return -1;        
    }

    if (connect(backend_fd, (struct sockaddr *)&backend_addr, sizeof(&backend_addr)) < 0)
    {
        perror("connect");
        close(backend_fd);
        return -1;       
    }

    if (set_nonb(backend_fd) < 0)
    {
        close(backend_fd);
        return -1;
    }

    return backend_fd;
}
