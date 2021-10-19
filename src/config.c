#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "config.h"


struct shart_config * parse_args_to_config(int argc, char * argv[]) {
    struct shart_config * config = malloc(sizeof(struct shart_config));
    config->srt_publish_port = DEFAULT_SRT_PUBLISH_PORT;
    config->srt_publish_passphrase = DEFAULT_SRT_PUBLISH_PASSPHRASE;
    config->srt_subscribe_port = DEFAULT_SRT_SUBSCRIBE_PORT;
    config->srt_subscribe_passphrase = DEFAULT_SRT_SUBSCRIBE_PASSPHRASE;
    config->web_port = DEFAULT_WEB_PORT;
    config->max_publishers = DEFAULT_MAX_PUBLISHERS;
    config->max_subscribers_per_publisher = DEFAULT_MAX_SUBSCRIBERS_PER_PUBLISHER;
    config->max_pending_connections = DEFAULT_MAX_PENDING_CONNECTIONS;
    config->auth_command = DEFAULT_AUTH_COMMAND;
    config->read_web_ip_from_headers = DEFAULT_READ_WEB_IP_FROM_HEADERS;

    for (int i = 1; i < argc; i += 2) {
        char * flag = argv[i];
        if (argc < i + 1) goto error;
        char * param = argv[i+1];
        if (strcmp(flag, "--srt-publish-port") == 0) {
            // SRT publish port
            config->srt_publish_port = atoi(param);
            assert(config->srt_publish_port != 0);
        } else if (strcmp(flag, "--srt-publish-passphrase") == 0) {
            // SRT publish passphrase
            config->srt_publish_passphrase = param;
        } else if (strcmp(flag, "--srt-subscribe-port") == 0) {
            // SRT subscribe port
            config->srt_subscribe_port = atoi(param);
            assert(config->srt_subscribe_port != 0);
        } else if (strcmp(flag, "--srt-subscribe-passphrase") == 0) {
            // SRT subscribe passphrase
            config->srt_subscribe_passphrase = param;
        } else if (strcmp(flag, "--web-port") == 0) {
            // Web Port
            config->web_port = atoi(param);
            assert(config->web_port != 0);
        } else if (strcmp(flag, "--max-streams") == 0) {
            // Max publishers
            config->max_publishers = atoi(param);
        } else if (strcmp(flag, "--max-subscribers") == 0) {
            // Max subscribers
            config->max_subscribers_per_publisher = atoi(param);
        } else if (strcmp(flag, "--max-pending-connections") == 0) {
            // Max pending connections
            config->max_pending_connections = atoi(param);
        } else if (strcmp(flag, "--auth-command") == 0) {
            // Auth command
            config->auth_command = param;
        } else if (strcmp(flag, "--read-web-ip-from-headers") == 0) {
            // Read web IP address from headers
            config->read_web_ip_from_headers = param[0] == 'y';
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
