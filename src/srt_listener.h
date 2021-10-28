#ifndef SRT_LISTEN_H_
#define SRT_LISTEN_H_

#include "authenticator.h"
#include "published_stream.h"

#ifndef SRT_LISTEN_BACKLOG
#define SRT_LISTEN_BACKLOG 4
#endif

void start_srt_listeners(
        unsigned short publish_port, unsigned short subscribe_port,
        char * publish_passphrase, char * subscribe_passphrase,
        struct authenticator * auth, struct published_stream_map * map);

#endif
