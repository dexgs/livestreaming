#include <assert.h>
#include <pthread.h>

#include "authenticator.h"
#include "published_stream.h"
#include "srt.h"
#include "srt_common.h"
#include "srt_subscriber.h"


void * srt_subscriber(void * _d) {
    struct srt_thread_data * d = (struct srt_thread_data *) _d;
    SRTSOCKET sock = d->sock;
    char * addr = d->addr;
    struct authenticator * auth = d->auth;
    struct published_stream_map * map = d->map;
    free(d);

    char * name = get_socket_stream_id(sock);

    add_srt_subscriber(map, auth, name, addr, sock);

    return NULL;
}
