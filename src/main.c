#include <signal.h>
#include <stdio.h>
#include "config.h"
#include "authenticator.h"
#include "published_stream.h"
#include "srt_listener.h"

int main(int argc, char * argv[]) {
    struct shart_config * c = parse_args_to_config(argc, argv);
    struct authenticator * auth = create_authenticator(
            c->auth_command, c->max_pending_connections);
    struct published_stream_map * map = create_published_stream_map(
            c->max_publishers, c->max_subscribers_per_publisher);

    start_srt_listeners(
            c->srt_publish_port, c->srt_subscribe_port,
            c->srt_publish_passphrase, c->srt_subscribe_passphrase,
            auth, map);

    // start_web_listener(c->web_port, auth, map);

    // Block until interrupted
    sigset_t set;
    int sig = 2;
    sigwait(&set, &sig);
}
