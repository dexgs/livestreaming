#ifndef WEB_LISTENER_H_
#define WEB_LISTENER_H_

#include <stdbool.h>
#include "authenticator.h"
#include "published_stream.h"

void start_web_listener(
        unsigned short port, bool read_web_ip_from_headers,
        struct authenticator * auth, struct published_stream_map * map);

#endif
