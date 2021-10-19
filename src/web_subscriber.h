#ifndef WEB_SUBSCRIBER_H_
#define WEB_SUBSCRIBER_H_

#include <stdbool.h>
#include "authenticator.h"
#include "published_stream.h"

void web_subscriber(
        int sock, bool read_web_ip_from_headers, char * addr,
        struct authenticator * auth, struct published_stream_map * map);

int write_to_web_subscriber(
                int sock, char * hex, unsigned int hex_len,
                char * data, unsigned int data_len);

// Write hex bytes into `buf`
void get_len_hex(
                unsigned int data_len, char * buf, unsigned int buf_len,
                unsigned int * hex_len);

#endif
