#ifndef WEB_LISTENER_H_
#define WEB_LISTENER_H_

#include <stdbool.h>
#include "authenticator.h"
#include "published_stream.h"

#ifndef WEB_LISTEN_BACKLOG
#define WEB_LISTEN_BACKLOG 100
#endif

#ifndef WEB_SEND_BUFFER_SIZE
#define WEB_SEND_BUFFER_SIZE 1490944
#endif

void start_web_listener(
        unsigned short port, bool read_web_ip_from_headers,
        struct authenticator * auth, struct published_stream_map * map);

#endif
