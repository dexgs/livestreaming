#ifndef SHART_CONFIG_H_
#define SHART_CONFIG_H_

#include <stdbool.h>

#define DEFAULT_SRT_PUBLISH_PORT 9991
#define DEFAULT_SRT_PUBLISH_PASSPHRASE ""
#define DEFAULT_SRT_SUBSCRIBE_PORT 1234
#define DEFAULT_SRT_SUBSCRIBE_PASSPHRASE ""
#define DEFAULT_WEB_PORT 8071
#define DEFAULT_MAX_PUBLISHERS 0
#define DEFAULT_MAX_SUBSCRIBERS_PER_PUBLISHER 0
#define DEFAULT_MAX_PENDING_CONNECTIONS 0
#define DEFAULT_AUTH_COMMAND "exit 0"
#define DEFAULT_READ_WEB_IP_FROM_HEADERS false

#define HELP_MESSAGE "USAGE:\n\
--srt-publish-port <PORT>            set port to use listen for SRT publishers \n\
\n\
--srt-publish-passphrase <STRING>    set SRT encryption passphrase for publishers \n\
                                     (must be between 10 and 79 characters long) \n\
\n\
--srt-subscribe-port <PORT>          set port to use for SRT subscribers \n\
\n\
--srt-subscribe-passphrase <STRING>  set SRT encryption passphrase for subscribers \n\
                                     (must be between 10 and 79 characters long) \n\
\n\
--web-port <PORT>                    set port to use for HTTP \n\
\n\
--max-streams <NUMBER>               Maximum number of streams allowed to be \n\
                                        published at once (0 for no limit) \n\
\n\
--max-subscribers <NUMBER>           Maximum number of subscribers allowed \n\
                                        for an individual stream (0 for no limit) \n\
\n\
--max-pending-connections <NUMBER>   Maximum number of pending connections \n\
                                        (0 for no limit) \n\
\n\
--auth-command <COMMAND>             Command to execute to authenticate connections\n\
\n\
--read-web-ip-from-headers <y/n>     If this option is passed, parse the IP address\n\
                                        for web connections from requested headers.\n"



// Overall configuration for ShaRT. Gets set once
// at startup based on command line arguments (see HELP_MESSAGE)
struct shart_config {
    unsigned short srt_publish_port;
    char * srt_publish_passphrase;
    unsigned short srt_subscribe_port;
    char * srt_subscribe_passphrase;
    unsigned short web_port;
    // The maximum number of streams allowed to be published at one time
    unsigned int max_publishers;
    // The maximum number of subscribers allowed for any published stream
    unsigned int max_subscribers_per_publisher;
    // The maximum number of pending connections at one time
    unsigned int max_pending_connections;
    // Command to execute to authenticate publish and subscribe requests
    char * auth_command;
    // Whether or not to read web IP from request headers
    bool read_web_ip_from_headers;
};

// Assigns values to shart_config given command line args
struct shart_config * parse_args_to_config(int argc, char * argv[]);

#endif
