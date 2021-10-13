#ifndef SRT_PUBLISHER_H_
#define SRT_PUBLISHER_H_

#include "thirdparty/srt/srt.h"
#include "published_stream.h"
#include "authenticator.h"

void srt_publisher(
        SRTSOCKET sock, char *  addr, struct authenticator * auth,
        struct published_stream_map * map);

#endif
