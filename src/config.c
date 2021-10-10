#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "management_types.h"


struct shart_config * parse_args_to_config(int argc, char * argv[]) {
    struct shart_config * config = malloc(sizeof(struct shart_config));
    config->srt_port = DEFAULT_SRT_PORT;
    config->web_port = DEFAULT_WEB_PORT;
    config->max_publishers = DEFAULT_MAX_PUBLISHERS;
    config->max_subscribers_per_publisher = DEFAULT_MAX_SUBSCRIBERS_PER_PUBLISHER;
    config->max_pending_connections = DEFAULT_MAX_PENDING_CONNECTIONS;
    config->auth_command = DEFAULT_AUTH_COMMAND;

    for (int i = 1; i < argc; i += 2) {
        char * flag = argv[i];
        if (argc < i + 1) goto error;
        char * param = argv[i+1];
        if (strcmp(flag, "-s") == 0) {
            // SRT Port
            config->srt_port = atoi(param);
            assert(config->srt_port != 0);
        } else if (strcmp(flag, "-w") == 0) {
            // WebRTC Port
            config->web_port = atoi(param);
            assert(config->web_port != 0);
        } else if (strcmp(flag, "-p") == 0) {
            // Max publishers
            config->max_publishers = atoi(param);
        } else if (strcmp(flag, "-S") == 0) {
            // Max subscribers
            config->max_subscribers_per_publisher = atoi(param);
        } else if (strcmp(flag, "-P") == 0) {
            // Max pending connections
            config->max_pending_connections = atoi(param);
        } else if (strcmp(flag, "-a") == 0) {
            // Auth command
            config->auth_command = param;
        } else {
            goto error;
        }
    }

    return config;

error:
    free(config);
    printf(HELP_MESSAGE);
    exit(1);
}
