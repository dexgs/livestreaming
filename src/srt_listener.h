#ifndef SRT_LISTEN_H_
#define SRT_LISTEN_H_

#include "authenticator.h"
#include "published_stream.h"

#ifndef SRT_LISTEN_BACKLOG
#define SRT_LISTEN_BACKLOG 4
#endif

#ifndef MIN_PACKETS_IN_FLIGHT
#define MIN_PACKETS_IN_FLIGHT 1024
#endif

#ifndef SEND_BUFFER_SIZE
#define SEND_BUFFER_SIZE 1490944
#endif

#ifndef RECV_BUFFER_SIZE
#define RECV_BUFFER_SIZE 1490944
#endif

#ifndef OVERHEAD_BW_PERCENT
#define OVERHEAD_BW_PERCENT 10
#endif

#ifndef LATENCY_MS
#define LATENCY_MS 0
#endif

#ifndef ENABLE_TIMESTAMPS
#define ENABLE_TIMESTAMPS false
#endif

#ifndef ENABLE_REPEATED_LOSS_REPORTS
#define ENABLE_REPEATED_LOSS_REPORTS false
#endif

#ifndef ENABLE_DRIFT_TRACER
#define ENABLE_DRIFT_TRACER false
#endif

void start_srt_listeners(
        unsigned short publish_port, unsigned short subscribe_port,
        char * publish_passphrase, char * subscribe_passphrase,
        struct authenticator * auth, struct published_stream_map * map);

#endif
