#ifndef SRT_LISTEN_H_
#define SRT_LISTEN_H_

#include "authenticator.h"
#include "published_stream.h"

#ifndef SRT_LISTEN_BACKLOG
#define SRT_LISTEN_BACKLOG 100
#endif

#ifndef MAX_PACKETS_IN_FLIGHT
#define MAX_PACKETS_IN_FLIGHT 25600
#endif

#ifndef SEND_BUFFER_SIZE
#define SEND_BUFFER_SIZE 1500000
#endif

#ifndef RECV_BUFFER_SIZE
#define RECV_BUFFER_SIZE 1500000
#endif

#ifndef RECV_LATENCY_MS
#define RECV_LATENCY_MS 60
#endif

#ifndef ENABLE_TIMESTAMPS
#define ENABLE_TIMESTAMPS true
#endif

#ifndef ENABLE_REPEATED_LOSS_REPORTS
#define ENABLE_REPEATED_LOSS_REPORTS true
#endif

#ifndef ENABLE_DRIFT_TRACER
#define ENABLE_DRIFT_TRACER true
#endif

void start_srt_listeners(
        unsigned short publish_port, unsigned short subscribe_port,
        char * publish_passphrase, char * subscribe_passphrase,
        struct authenticator * auth, struct published_stream_map * map);

#endif
