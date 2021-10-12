#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include "srt_listener.h"
#include "authenticator.h"
#include "published_stream.h"
#include "thirdparty/srt/srt.h"

struct thread_data {
    SRTSOCKET sock;
    struct authenticator * auth;
    struct published_stream_map * map;
    bool is_publisher;
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
        struct published_stream_map * map, bool is_publisher);

void * run_srt_listener(void * d);

void start_srt_listeners(
        unsigned short publish_port, unsigned short subscribe_port,
        struct authenticator * auth, struct published_stream_map * map)
{
    srt_startup();

    printf("Listening for SRT publishers on port: %d\n", publish_port);
    start_srt_listener(publish_port, auth, map, true);

    printf("Listening for SRT subscribers on port: %d\n", subscribe_port);
    start_srt_listener(subscribe_port, auth, map, false);
}

void start_srt_listener(
        unsigned short port, struct authenticator * auth,
        struct published_stream_map * map, bool is_publisher)
{
    struct sockaddr_in addr = get_sockaddr_with_port(port);

    SRTSOCKET sock = srt_create_socket();
    int bind_err = srt_bind(sock, (struct sockaddr *) &addr, sizeof(addr));
    assert(bind_err != SRT_ERROR);

    struct thread_data * d = malloc(sizeof(struct thread_data));
    d->sock = sock;
    d->auth = auth;
    d->map = map;
    d->is_publisher = is_publisher;

    pthread_t thread_handle;
    pthread_create(&thread_handle, NULL, run_srt_listener, d);
}


#ifndef SRT_LISTEN_BACKLOG
#define SRT_LISTEN_BACKLOG 10
#endif

void * run_srt_listener(void * _d) {
    struct thread_data * d = (struct thread_data *) _d;
    SRTSOCKET sock = d->sock;
    struct authenticator * auth = d->auth;
    struct published_stream_map * map = d->map;
    free(d);

    int listen_err = srt_listen(sock, SRT_LISTEN_BACKLOG);
    assert(listen_err != SRT_ERROR);

    while (true) {
        struct sockaddr_storage client_addr;
        int client_addr_len = sizeof(client_addr);

        SRTSOCKET client_sock =
            srt_accept(sock, (struct sockaddr *) &client_addr, &client_addr_len);
        
        if (max_pending_connections_exceeded(auth)) {
            int close_err;
            close_err = srt_close(client_sock);
            assert(close_err != SRT_ERROR);
        } else {
            /*
            printf("huzzah!\n");

            int recv_err = 0;
            char buf[8192];
            while (recv_err != SRT_ERROR) {
                recv_err = srt_recv(client_sock, buf, sizeof(buf));
                printf("recvd\n");
            }
            fprintf(stderr, "err: %s\n", srt_getlasterror_str());
            srt_close(client_sock);

            printf("DONE!\n");
            */
        }
    }
}
