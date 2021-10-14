#include <pthread.h>
#include "srt_subscriber.h"
#include "srt_common.h"
#include "thirdparty/srt/srt.h"
#include "published_stream.h"
#include "authenticator.h"


void * srt_subscriber(void * _d) {
    struct srt_thread_data * d = (struct srt_thread_data *) _d;
    SRTSOCKET sock = d->sock;
    char * addr = d->addr;
    struct authenticator * auth = d->auth;
    struct published_stream_map * map = d->map;
    free(d);

    char * name = get_socket_stream_id(sock);

    char * processed_name = authenticate(auth, false, addr, name);
    free(name);
    name = processed_name;

    // Close connection prematurely if...
    if (
            // If authentication failed
            name == NULL
            // If there is no active stream with the requested name
            || !stream_name_in_map(map, name)
            // If the maximum number of subscribers to the stream
            // would be exceeded
            || max_subscribers_exceeded(map, name))
    {
        srt_close(sock);
        return NULL;
    }


    struct published_stream_data * data = get_stream_from_map(map, name);
    if (data != NULL) {
        add_srt_subscriber(data, sock);
    } else {
        srt_close(sock);
    }

    return NULL;
}
