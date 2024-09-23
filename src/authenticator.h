#ifndef AUTHENTICATOR_H_
#define AUTHENTICATOR_H_

#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

struct authenticator;

struct authenticator * create_authenticator(
        const char * auth_command, unsigned int max_pending_connections);

// Returns the processed stream name on success, returns NULL on failure
char * authenticate(
        struct authenticator * auth, bool is_publisher,
        const char * addr, const char * stream_name);

char * sockaddr_to_string(struct sockaddr * addr, int addr_len);

#endif
