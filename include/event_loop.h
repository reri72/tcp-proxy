#ifndef _EVENT_LOOP_H_
#define _EVENT_LOOP_H_

int setup_signals(void);

int set_epoll(int *epfd, int servfd);

int run_event_loop(int epfd, int servfd);

#endif