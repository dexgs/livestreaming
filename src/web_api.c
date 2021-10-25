#include <pthread.h>
#include <stdio.h>
#include "web_api.h"
#include "published_stream.h"

struct web_api_data {
    char ** names;
};

struct web_api_data * create_web_api_data() {
    struct web_api_data * data = malloc(sizeof(struct web_api_data));

    data->names = NULL;

    return data;
}

void active_stream_list() {
}

void single_stream_data() {
}

void web_api(
        struct published_stream_map * map, struct web_api_data * data,
        const char * path, int sock)
{
    close(sock);
}
