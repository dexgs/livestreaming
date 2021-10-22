#include <pthread.h>
#include <stdbool.h>
#include <assert.h>
#include "srt_common.h"
#include "authenticator.h"
#include "published_stream.h"

void start_srt_thread(
        SRTSOCKET sock, char * addr, struct authenticator * auth,
        struct published_stream_map * map, void * (*thread_function)(void *))
{
    struct srt_thread_data * d = malloc(sizeof(struct srt_thread_data));
    d->sock = sock;
    d->addr = addr;
    d->auth = auth;
    d->map = map;

    int pthread_err;
    pthread_t thread_handle;

    pthread_err = pthread_create(&thread_handle, NULL, thread_function, d);
    assert(pthread_err == 0);

    pthread_err = pthread_detach(thread_handle);
    assert(pthread_err == 0);
}

#define SRT_STREAMID_MAX_LEN 512

char * get_socket_stream_id(SRTSOCKET sock) {
    char * name = malloc(SRT_STREAMID_MAX_LEN);
    int name_len = SRT_STREAMID_MAX_LEN;
    srt_getsockflag(sock, SRTO_STREAMID, name, &name_len);
    name = realloc(name, name_len + 1);

    return name;
}
