#include <stdio.h>
#include "thirdparty/srt/srt.h"
#include "config.h"
#include "authenticator.h"

int main(int argc, char * argv[]) {
    struct shart_config * c = parse_args_to_config(argc, argv);
    struct authenticator * auth = create_authenticator(
            c->auth_command, c->max_pending_connections);

    SRTSOCKET sock = srt_create_socket();
    return srt_close(sock);
}
