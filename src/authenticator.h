#ifndef AUTHENTICATOR_H_
#define AUTHENTICATOR_H_

#include <stdbool.h>

#define PUBLISH_STRING "PUBLISH"
#define SUBSCRIBE_STRING "SUBSCRIBE"

struct authenticator;

struct authenticator * create_authenticator(
        const char * auth_command, unsigned int max_pending_connections);

// Returns the processed stream name on success, returns NULL on failure
char * authenticate(
        struct authenticator * auth, bool is_publisher,
        const char * addr, const char * stream_name);

bool max_pending_connections_exceeded(struct authenticator * auth);

#endif
