#include <stdio.h>
#include "thirdparty/srt/srt.h"
#include "config.h"

int main(int argc, char * argv[]) {
    struct shart_config * c = parse_args_to_config(argc, argv);

    SRTSOCKET sock = srt_create_socket();
    return srt_close(sock);
}
