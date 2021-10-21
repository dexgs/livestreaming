#include <sys/socket.h>
#include <assert.h>
#include <stdio.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include "published_stream.h"
#include "authenticator.h"
#include "web_listener.h"
#include "web_subscriber.h"

#ifndef WEB_LISTEN_BACKLOG
#define WEB_LISTEN_BACKLOG 10
#endif

struct thread_data {
    int sock;
    bool read_web_ip_from_headers;
    struct authenticator * auth;
    struct published_stream_map * map;
};

void * run_web_listener(void * _d);

void start_web_listener(
        unsigned short port, bool read_web_ip_from_headers,
        struct authenticator * auth, struct published_stream_map * map)
{
    // Stop the program from closing when http subscribers close the connection
    signal(SIGPIPE, SIG_IGN);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));

    int bind_err = bind(sock, (struct sockaddr *) &addr, sizeof(addr));
    assert(bind_err == 0);

    int listen_err = listen(sock, WEB_LISTEN_BACKLOG);
    assert(listen_err == 0);

    printf("Listening for web subscribers on port: %d\n", port);

    struct thread_data * d = malloc(sizeof(struct thread_data));
    d->sock = sock;
    d->read_web_ip_from_headers = read_web_ip_from_headers;
    d->auth = auth;
    d->map = map;

    pthread_t thread_handle;
    int pthread_err;

    pthread_err = pthread_create(&thread_handle, NULL, run_web_listener, d);
    assert(pthread_err == 0);

    pthread_err = pthread_detach(thread_handle);
    assert(pthread_err == 0);
}

void * run_web_listener(void * _d) {
    struct thread_data * d = (struct thread_data *) _d;
    int sock = d->sock;
    bool read_web_ip_from_headers = d->read_web_ip_from_headers;
    struct authenticator * auth = d->auth;
    struct published_stream_map * map = d->map;
    free(d);

    while (true) {
        struct sockaddr_in client_addr;
        unsigned int client_addr_len;

        int client_sock = accept(
                sock, (struct sockaddr *) &client_addr, &client_addr_len);
        if (client_sock == -1) continue;

        if (max_pending_connections_exceeded(auth)) {
            close(client_sock);
        } else {
            char * addr_str = sockaddr_to_string(
                    (struct sockaddr *) &client_addr, client_addr_len);
            web_subscriber(client_sock, read_web_ip_from_headers, addr_str, auth, map);
        }
    }

    return NULL;
}
