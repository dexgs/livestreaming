#ifndef SRT_LISTEN_H_
#define SRT_LISTEN_H_

#include "authenticator.h"
#include "published_stream.h"

void start_srt_listener(
        unsigned short port, struct authenticator * auth, 
        struct published_stream_map * map);

#endif
