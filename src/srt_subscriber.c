#include <pthread.h>
#include <assert.h>
#include "srt_subscriber.h"
#include "srt_common.h"
#include "srt.h"
#include "published_stream.h"
#include "authenticator.h"


void * srt_subscriber(void * _d) {
    struct srt_thread_data * d = (struct srt_thread_data *) _d;
    SRTSOCKET sock = d->sock;
    char * addr = d->addr;
    struct authenticator * auth = d->auth;
    struct published_stream_map * map = d->map;
    free(d);

    int ms = 10;
    int set_flag_err = srt_setsockflag(sock, SRTO_SNDTIMEO, &ms, sizeof(ms));
    assert(set_flag_err == 0);

    char * name = get_socket_stream_id(sock);

    add_srt_subscriber(map, auth, name, addr, sock);

    return NULL;
}
