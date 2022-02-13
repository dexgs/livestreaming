#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include "srt_listener.h"
#include "srt_publisher.h"
#include "srt_subscriber.h"
#include "srt_common.h"
#include "authenticator.h"
#include "published_stream.h"
#include "srt.h"

struct thread_data {
    SRTSOCKET sock;
    struct authenticator * auth;
    struct published_stream_map * map;
    bool is_publisher;
    struct sockaddr_in addr;
};


struct sockaddr_in get_sockaddr_with_port(unsigned short port) {
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    return addr;
}


void start_srt_listener(
        unsigned short port, struct authenticator * auth,
        struct published_stream_map * map, bool is_publisher,
        const char * passphrase);

void * run_srt_listener(void * d);

void start_srt_listeners(
        unsigned short publish_port, unsigned short subscribe_port,
        char * publish_passphrase, char * subscribe_passphrase,
        struct authenticator * auth, struct published_stream_map * map)
{
    srt_startup();

    printf("Listening for SRT publishers on port: %d\n", publish_port);
    start_srt_listener(publish_port, auth, map, true, publish_passphrase);

    printf("Listening for SRT subscribers on port: %d\n", subscribe_port);
    start_srt_listener(subscribe_port, auth, map, false, subscribe_passphrase);
}

// Set socket flags that are unchanging, i.e. the passphrase is not set in this
// function because it depends on a parameter that is not known at compile time.
void set_sock_flags(SRTSOCKET sock) {
    int set_flag_err;

    int max_fc = MAX_PACKETS_IN_FLIGHT;
    // Set maximum number of "in-flight packets"
    set_flag_err = srt_setsockflag(sock, SRTO_FC, &max_fc, sizeof(max_fc));
    assert(set_flag_err != SRT_ERROR);

    int send_buf = SEND_BUFFER_SIZE;
    // Set minimum buffer sizes
    set_flag_err =
        srt_setsockflag(sock, SRTO_SNDBUF, &send_buf, sizeof(send_buf));
    assert(set_flag_err != SRT_ERROR);

    int recv_buf = RECV_BUFFER_SIZE;
    set_flag_err =
        srt_setsockflag(sock, SRTO_RCVBUF, &recv_buf, sizeof(recv_buf));
    assert(set_flag_err != SRT_ERROR);

    int oh_percent = OVERHEAD_BW_PERCENT;
    // Set what percentage of bandwidth is allowed to be used for overhead
    set_flag_err =
        srt_setsockflag(sock, SRTO_OHEADBW, &oh_percent, sizeof(oh_percent));
    assert(set_flag_err != SRT_ERROR);

    struct linger l = {.l_onoff = 0, .l_linger = 0};
    // Disable socket linger
    set_flag_err = srt_setsockflag(sock, SRTO_LINGER, &l, sizeof(l));
    assert(set_flag_err != SRT_ERROR);

    // Set latency
    int ms = LATENCY_MS;
    set_flag_err = srt_setsockflag(sock, SRTO_LATENCY, &ms, sizeof(ms));
    assert(set_flag_err != SRT_ERROR);

    bool enable_tsbpd = ENABLE_TIMESTAMPS;
    // Set timestamps toggle
    set_flag_err =
        srt_setsockflag(sock, SRTO_TSBPDMODE, &enable_tsbpd, sizeof(enable_tsbpd));
    assert(set_flag_err != SRT_ERROR);

    bool enable_nakreport = ENABLE_REPEATED_LOSS_REPORTS;
    // Set repeated loss detection reports toggle
    set_flag_err =
        srt_setsockflag(sock, SRTO_NAKREPORT, &enable_nakreport, sizeof(enable_nakreport));
    assert(set_flag_err != SRT_ERROR);

    bool enable_drift = ENABLE_DRIFT_TRACER;
    // Set drift tracer toggle
    set_flag_err =
        srt_setsockflag(sock, SRTO_DRIFTTRACER, &enable_drift, sizeof(enable_drift));
    assert(set_flag_err != SRT_ERROR);
}


void start_srt_listener(
        unsigned short port, struct authenticator * auth,
        struct published_stream_map * map, bool is_publisher,
        const char * passphrase)
{
    struct sockaddr_in addr = get_sockaddr_with_port(port);

    SRTSOCKET sock = srt_create_socket();

    set_sock_flags(sock);

    // Set passphrase for encryption
    int set_passphrase_err =
        srt_setsockflag(sock, SRTO_PASSPHRASE, passphrase, strlen(passphrase));
    assert(set_passphrase_err != SRT_ERROR);

    int bind_err = srt_bind(sock, (struct sockaddr *) &addr, sizeof(addr));
    assert(bind_err != SRT_ERROR);

    struct thread_data * d = malloc(sizeof(struct thread_data));
    d->sock = sock;
    d->auth = auth;
    d->map = map;
    d->is_publisher = is_publisher;
    d->addr = addr;

    int pthread_err;
    pthread_t thread_handle;

    pthread_err = pthread_create(&thread_handle, NULL, run_srt_listener, d);
    assert(pthread_err == 0);

    pthread_err = pthread_detach(thread_handle);
    assert(pthread_err == 0);
}


void * run_srt_listener(void * _d) {
    struct thread_data * d = (struct thread_data *) _d;
    SRTSOCKET sock = d->sock;
    struct authenticator * auth = d->auth;
    struct published_stream_map * map = d->map;
    bool is_publisher = d->is_publisher;
    struct sockaddr_in client_addr = d->addr;
    free(d);

    int listen_err = srt_listen(sock, SRT_LISTEN_BACKLOG);
    assert(listen_err != SRT_ERROR);

    int client_addr_len = sizeof(client_addr);

    while (true) {

        SRTSOCKET client_sock =
            srt_accept(sock, (struct sockaddr *) &client_addr, &client_addr_len);

        if (client_sock < 0) continue;
        
        if (max_pending_connections_exceeded(auth)) {
            int close_err;
            close_err = srt_close(client_sock);
            assert(close_err != SRT_ERROR);
        } else {
            char * addr_str = sockaddr_to_string(
                    (struct sockaddr *) &client_addr, client_addr_len);
            if (is_publisher) {
                start_srt_thread(
                        client_sock, addr_str, auth, map, srt_publisher);
            } else {
                start_srt_thread(
                        client_sock, addr_str, auth, map, srt_subscriber);
            }
        }
    }

    return NULL;
}
