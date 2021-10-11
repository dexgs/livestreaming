#ifndef AUTHENTICATOR_H_
#define AUTHENTICATOR_H_

#include <stdbool.h>

#define PUBLISH_STRING "PUBLISH"
#define SUBSCRIBE_STRING "SUBSCRIBE"

enum connection_type {publish, subscribe};

struct authenticator;

struct authenticator * create_authenticator(
        const char * auth_command, unsigned int max_pending_connections);

bool authenticate(
        struct authenticator * auth, enum connection_type type,
        const char * stream_name, const char * password, const char * addr);

bool max_pending_connections_exceeded(struct authenticator * auth);

#endif
