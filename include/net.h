#ifndef _NET_H_
#define _NET_H_

int set_nonb(int fd);

int set_socket(int *fd);

int connect_backend(const char *ip, int port);

#endif