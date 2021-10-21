#ifndef SRT_COMMON_H_
#define SRT_COMMON_H_

#include "srt/srt.h"
#include "published_stream.h"
#include "authenticator.h"

// MPEGTS packet size is 188. 188 x 7 = 1316, i.e. the most complete MPEGTS
// packets that can be sent in a single UDP packet (1500 bytes)
#define SRT_BUFFER_SIZE 1316

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
