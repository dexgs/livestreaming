#ifndef SRT_COMMON_H_
#define SRT_COMMON_H_

#include "thirdparty/srt/srt.h"
#include "published_stream.h"
#include "authenticator.h"

struct srt_thread_data {
    SRTSOCKET sock;
    char * addr;
    struct authenticator * auth;
    struct published_stream_map * map;
};

void start_srt_thread(
        SRTSOCKET sock, char * addr, struct authenticator * auth,
        struct published_stream_map * map, void * (*thread_function)(void *));

char * get_socket_stream_id(SRTSOCKET sock);

#endif
