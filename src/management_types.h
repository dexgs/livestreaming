#ifndef MANAGEMENT_TYPES_H_
#define MANAGEMENT_TYPES_H_

#include <sys/socket.h>

#define DEFAULT_SRT_PORT 9071
#define DEFAULT_WEB_PORT 8071
#define DEFAULT_MAX_PUBLISHERS 0
#define DEFAULT_MAX_SUBSCRIBERS_PER_PUBLISHER 0
#define DEFAULT_MAX_PENDING_CONNECTIONS 0
#define DEFAULT_AUTH_COMMAND "exit"

#define HELP_MESSAGE "USAGE:\n\
-s <PORT>    set port to use for SRT \n\
-w <PORT>    set port to use for WebRTC \n\
-p <NUMBER>  Maximum number of streams allowed\
to be published at once (0 for no limit) \n\
-S <NUMBER>  Maximum number of subscribers \
allowed for an individual stream (0 for no limit) \n\
-P <NUMBER>  Maximum number of pending connections(0 for no limit) \n\
-a <COMMAND> Command to execute to authenticate connections\n"

struct shart_config {
    unsigned short srt_port;
    unsigned short web_port;
    // The maximum number of streams allowed to be published at one time
    unsigned int max_publishers;
    // The maximum number of subscribers allowed for any published stream
    unsigned int max_subscribers_per_publisher;
    // The maximum number of pending connections at one time
    unsigned int max_pending_connections;
    // Command to execute to authenticate publish and subscribe requests
    char * auth_command;
};

struct shart_config * parse_args_to_config(int argc, char * argv[]);

struct published_stream_data {
    char * stream_id;
    struct sockaddr * publisher;
    unsigned int num_subscribers;
};

#endif
