#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "guard.h"
#include "published_stream.h"
#include "web_api.h"

static const char * HTTP_JSON_OK =
    "HTTP/1.1 200 OK\r\n"
    "Connection: close\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Content-Type: application/json\r\n";

static const char * HTTP_CONTENT_LENGTH = "Content-Length: ";

static const char * HTTP_CHUNKED_ENCODING = "Transfer-Encoding: chunked\r\n";

static const char * HTTP_400 =
    "HTTP/1.1 400 Bad Request\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 21\r\n"
    "Connection: close\r\n\r\n"
    "Invalid API endpoint.";

static const char * HTTP_404 =
    "HTTP/1.1 404 Not Found\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 10\r\n"
    "Connection: close\r\n\r\n"
    "Not Found.";

// Limit the number of stream name list instances which may exist concurrently
static const size_t MAX_STREAM_NAMES_INSTANCES = 2;



struct web_api_data {
    pthread_mutex_t update_lock;
    pthread_mutex_t names_lock;
    struct stream_names * stream_names;
    size_t num_stream_names_instances;
};

// Store a list of stream names as well as the number of threads currently
// using these names. The last user must free this structure before exiting.
// Modifying this structure should be guarded by `web_api_data`'s `names_lock`.
struct stream_names {
    char ** names;
    unsigned int num_names;
    size_t num_requests;
};

struct web_api_data * create_web_api_data() {
    struct web_api_data * data = malloc(sizeof(struct web_api_data));

    *data = (struct web_api_data) {};

    int mutex_init_err = 0;
    mutex_init_err |= pthread_mutex_init(&data->update_lock, NULL);
    mutex_init_err |= pthread_mutex_init(&data->names_lock, NULL);
    assert(mutex_init_err == 0);

    return data;
}


// Update the active stream list in `data` using the current state of `map`.
// This function is thread-safe.
static void update_stream_list(
        struct published_stream_map * map, struct web_api_data * data)
{
    // Only one thread may generate the streams list at a time
    if (pthread_mutex_trylock(&data->update_lock) == 0) {
        bool more_instances_allowed;
        GUARD(&data->names_lock, {
            more_instances_allowed =
                data->num_stream_names_instances < MAX_STREAM_NAMES_INSTANCES;
        })

        if (more_instances_allowed) {
            unsigned int num_names = 0;
            char ** names = stream_names(map, &num_names);
            struct stream_names * stream_names = malloc(sizeof(struct stream_names));
            *stream_names = (struct stream_names) {
                .names = names,
                .num_names = num_names
            };

            GUARD(&data->names_lock, {
                data->stream_names = stream_names;
                data->num_stream_names_instances++;
            })
        }

        int mutex_lock_err = pthread_mutex_unlock(&data->update_lock);
        assert(mutex_lock_err == 0);
    }
}

// Return list of active streams sorted by number of viewers.
static void active_stream_list(
        struct published_stream_map * map, struct web_api_data * data,
        unsigned int num_streams, int sock)
{
    struct stream_names * stream_names;
    GUARD(&data->names_lock, {
        stream_names = data->stream_names;
        stream_names->num_requests++;
    })

    // Write the JSON-econded data to the socket, then close the connection
    write(sock, HTTP_JSON_OK, strlen(HTTP_JSON_OK));
    write(sock, HTTP_CHUNKED_ENCODING, strlen(HTTP_CHUNKED_ENCODING));
    write(sock, "\r\n", 2);

    write(sock, "1\r\n[\r\n", 6);

    // If the current number of streams is 0, or there is no streams list
    // already generated, make sure an empty list gets returned by preventing
    // the body of the for loop below from running.
    if (num_published_streams(map) == 0 || stream_names == NULL) {
        num_streams = 0;
    } else if (num_streams == 0) {
        // If num_streams was not set, return the full list of stream names.
        num_streams = stream_names->num_names;
    }

    char size_str[17];

    for (unsigned int i = 0; i < stream_names->num_names && i < num_streams; i++) {
        bool is_last_name = i + 1 >= stream_names->num_names || i + 1 >= num_streams;

        size_t chunk_size = strlen(stream_names->names[i]) + 2;
        if (!is_last_name) chunk_size++;

        snprintf(size_str, 17, "%lx", chunk_size);
        write(sock, size_str, strlen(size_str));
        write(sock, "\r\n", 2);
        write(sock, "\"", 1);
        write(sock, stream_names->names[i], strlen(stream_names->names[i]));
        write(sock, "\"", 1);

        if (!is_last_name) {
            write(sock, ",", 1);
        }
        write(sock, "\r\n", 2);
    }

    write(sock, "1\r\n]\r\n", 6);
    write(sock, "0\r\n\r\n", 5);

    close(sock);

    GUARD(&data->names_lock, {
        stream_names->num_requests--;
        // If there are no more users AND there are new stream names available
        if (
                stream_names->num_requests == 0
                && stream_names != data->stream_names)
        {
            free(stream_names);
            data->num_stream_names_instances--;
        }
    })

    update_stream_list(map, data);
}


// Send the number of watchers for a stream or 404 if the stream does not exist
static void single_stream_data(
        struct published_stream_map * map, const char * stream_name, int sock)
{
    struct published_stream_data * data = get_stream_from_map(map, stream_name);
    if (data == NULL) {
        write(sock, HTTP_404, strlen(HTTP_404));
    } else {
        unsigned int num_subscribers = get_num_subscribers(data);

        int mutex_lock_err = pthread_mutex_unlock(&data->access_lock);
        assert(mutex_lock_err == 0);

        write(sock, HTTP_JSON_OK, strlen(HTTP_JSON_OK));

        char num_subscribers_str[10];
        snprintf(num_subscribers_str, 10, "%u", num_subscribers);
        size_t str_len = strlen(num_subscribers_str);

        write(sock, HTTP_CONTENT_LENGTH, strlen(HTTP_CONTENT_LENGTH));

        char str_len_str[17];
        snprintf(str_len_str, 17, "%lx", str_len);

        write(sock, str_len_str, strlen(str_len_str));
        write(sock, "\r\n\r\n", 4);

        write(sock, num_subscribers_str, str_len);
    }

    close(sock);
}

const char * SINGLE_STREAM_PATH = "stream/";
const char * STREAM_LIST_PATH = "streams/";

void web_api(
        struct published_stream_map * map, struct web_api_data * data,
        char * path, unsigned int path_len, int sock)
{
    if (path[path_len - 1] == '/') {
        path[path_len - 1] = '\0';
    } else {
        path[path_len] = '\0';
    }

    if (
            path_len > strlen(SINGLE_STREAM_PATH)
            && strncmp(SINGLE_STREAM_PATH, path, strlen(SINGLE_STREAM_PATH)) == 0)
    {
        // Single stream
        path += strlen(SINGLE_STREAM_PATH);
        single_stream_data(map, path, sock);
    } else if (
            path_len > strlen(STREAM_LIST_PATH)
            && strncmp(STREAM_LIST_PATH, path, strlen(SINGLE_STREAM_PATH)) == 0)
    {
        // Stream list
        path += strlen(STREAM_LIST_PATH);
        unsigned int num_streams = strtoul(path, NULL, 10);
        active_stream_list(map, data, num_streams, sock);
    } else {
        write(sock, HTTP_400, strlen(HTTP_400));
    }

    close(sock);
}


void update_stream_list_timer(struct published_stream_map * map, struct web_api_data * data) {
    while (true) {
        update_stream_list(map, data);
        sleep(30);
    }
}
