#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include "srt_listener.h"
#include "authenticator.h"
#include "published_stream.h"
#include "thirdparty/srt/srt.h"

struct sockaddr_in get_sockaddr_with_port(unsigned short port) {
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = port;
    struct in_addr in;
    in.s_addr = inet_addr("0.0.0.0");
    addr.sin_addr = in;

    return addr;
}

void start_srt_listener(
        unsigned short port, struct authenticator * auth,
        struct published_stream_map * map)
{
    struct sockaddr_in addr = get_sockaddr_with_port(port);
 
    SRTSOCKET sock = srt_create_socket();
    int bind_err = srt_bind(sock, (struct sockaddr *) &addr, sizeof(addr));
    assert(bind_err == 0);
}

void run_srt_listener() {
}
