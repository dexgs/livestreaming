#ifndef WEB_API_H_
#define WEB_API_H_

#include "published_stream.h"

struct web_api_data;

struct web_api_data * create_web_api_data();

void web_api(
        struct published_stream_map * map, struct web_api_data * data,
        char * path, unsigned int path_len, int sock);

#endif
