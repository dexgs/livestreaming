#include <assert.h>
#include <signal.h>
#include <stdio.h>

#include "authenticator.h"
#include "published_stream.h"
#include "web_api.h"
#include "web_listener.h"
#include "web_subscriber.h"


struct thread_data {
    int sock;
    bool read_web_ip_from_headers;
    struct authenticator * auth;
    struct published_stream_map * map;
    struct sockaddr_in addr;
    struct web_api_data * data;
};

void * run_web_listener(void * _d);

void start_web_listener(
        unsigned short port, bool read_web_ip_from_headers,
        struct authenticator * auth, struct published_stream_map * map)
{
    // Stop the program from closing when http subscribers close the connection
    signal(SIGPIPE, SIG_IGN);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    int set_sock_opt_err = 0;

    int yes = 1;
    set_sock_opt_err |=
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    set_sock_opt_err |= 
        setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));

    assert(set_sock_opt_err == 0);

    int bind_err = bind(sock, (struct sockaddr *) &addr, sizeof(addr));
    assert(bind_err == 0);

    int listen_err = listen(sock, WEB_LISTEN_BACKLOG);
    assert(listen_err == 0);

#ifndef NDEBUG
    printf("Listening for web subscribers on port: %d\n", port);
#endif

    struct thread_data * d = malloc(sizeof(struct thread_data));
    d->sock = sock;
    d->read_web_ip_from_headers = read_web_ip_from_headers;
    d->auth = auth;
    d->map = map;
    d->addr = addr;
    d->data = create_web_api_data();

    pthread_t thread_handle;
    int pthread_err = 0;

    pthread_err |= pthread_create(&thread_handle, NULL, run_web_listener, d);
    pthread_err |= pthread_detach(thread_handle);

    assert(pthread_err == 0);
}


void * run_update_stream_list_timer(void * _d) {
    struct thread_data * d = (struct thread_data *) _d;
    struct published_stream_map * map = d->map;
    struct web_api_data * data = d->data;
    free(d);

    update_stream_list_timer(map, data);

    return NULL;
}


void * run_web_listener(void * _d) {
    struct thread_data * d = (struct thread_data *) _d;
    int sock = d->sock;
    bool read_web_ip_from_headers = d->read_web_ip_from_headers;
    struct authenticator * auth = d->auth;
    struct published_stream_map * map = d->map;
    struct sockaddr_in client_addr = d->addr;
    struct web_api_data * data = d->data;

    pthread_t thread_handle;
    int pthread_err;

    pthread_err = pthread_create(&thread_handle, NULL, run_update_stream_list_timer, d);
    assert(pthread_err == 0);

    pthread_err = pthread_detach(thread_handle);
    assert(pthread_err == 0);


    unsigned int client_addr_len = sizeof(client_addr);

    struct timeval timeout = { .tv_sec = 1, .tv_usec = 500000 };

    while (true) {
        int client_sock = accept(
                sock, (struct sockaddr *) &client_addr, &client_addr_len);
        if (client_sock < 0) continue;

        int set_sock_opt_err = 0;

        set_sock_opt_err |= setsockopt(
                client_sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        set_sock_opt_err |= setsockopt(
                client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        int buf_len = WEB_SEND_BUFFER_SIZE;
        set_sock_opt_err |= setsockopt(
                client_sock, SOL_SOCKET, SO_SNDBUF, &buf_len, sizeof(buf_len));

        int recv_buf_len = 1024;
        set_sock_opt_err |= setsockopt(
                client_sock, SOL_SOCKET, SO_RCVBUF, &recv_buf_len, sizeof(recv_buf_len));
        if (set_sock_opt_err != 0) {
            close(client_sock);
            continue;
        }


        if (max_pending_connections_exceeded(auth)) {
            close(client_sock);
        } else {
            char * addr_str = sockaddr_to_string(
                    (struct sockaddr *) &client_addr, client_addr_len);
            web_subscriber(
                    client_sock, read_web_ip_from_headers,
                    addr_str, auth, map, data);
        }
    }

    return NULL;
}
