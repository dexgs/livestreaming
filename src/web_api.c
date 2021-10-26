#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "web_api.h"
#include "published_stream.h"

const char * HTTP_JSON_OK =
    "HTTP/1.1 200 OK\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Content-Type: application/json\r\n";

const char * HTTP_CONTENT_LENGTH = "Content-Length: ";

const char * HTTP_CHUNKED_ENCODING = "Transfer-Encoding: chunked\r\n";

const char * HTTP_404 =
    "HTTP/1.1 404 Not Found\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 15\r\n"
    "Connection: close\r\n\r\n"
    "404 - Not Found";


struct web_api_data {
    unsigned int num_requests;
    pthread_mutex_t num_requests_lock;

    pthread_mutex_t cond_lock;
    pthread_cond_t names_swap_cond;
    bool skip_cond;
    pthread_mutex_t skip_cond_lock;

    unsigned int num_names;
    char ** names;
};

struct web_api_data * create_web_api_data() {
    struct web_api_data * data = malloc(sizeof(struct web_api_data));

    int mutex_init_err;
    mutex_init_err = pthread_mutex_init(&data->skip_cond_lock, NULL);
    assert(mutex_init_err == 0);
    mutex_init_err = pthread_mutex_init(&data->num_requests_lock, NULL);
    assert(mutex_init_err == 0);
    mutex_init_err = pthread_mutex_init(&data->cond_lock, NULL);
    assert(mutex_init_err == 0);
    int cond_init_err;
    cond_init_err = pthread_cond_init(&data->names_swap_cond, NULL);
    assert(cond_init_err == 0);


    data->num_names = 0;
    data->names = NULL;
    data->skip_cond = false;

    return data;
}


void active_stream_list(
        struct published_stream_map * map, struct web_api_data * data,
        unsigned int num_streams, int sock)
{
    int mutex_lock_err;
    int cond_err;

    // If no other thread is generating the streams list
    bool generate_stream_names_list =
        pthread_mutex_trylock(&data->cond_lock) == 0;

    mutex_lock_err = pthread_mutex_lock(&data->num_requests_lock);
    assert(mutex_lock_err == 0);

    if (!generate_stream_names_list && data->num_requests == 0) {
        // Wait for `data->names` and `data->num_names` to be updated
        mutex_lock_err = pthread_mutex_lock(&data->cond_lock);
        assert(mutex_lock_err == 0);
        mutex_lock_err = pthread_mutex_unlock(&data->cond_lock);
        assert(mutex_lock_err == 0);
    }
    data->num_requests++;

    mutex_lock_err = pthread_mutex_unlock(&data->num_requests_lock);
    assert(mutex_lock_err == 0);

    write(sock, HTTP_JSON_OK, strlen(HTTP_JSON_OK));
    write(sock, HTTP_CHUNKED_ENCODING, strlen(HTTP_CHUNKED_ENCODING));
    write(sock, "\r\n", 2);

    write(sock, "1\r\n[\r\n", 6);

    char * size_str = malloc(10);
    if (num_streams == 0) num_streams = data->num_names;
    for (unsigned int i = 0; i < data->num_names && i < num_streams; i++) {
        bool is_last_name = i + 1 >= data->num_names || i + 1 >= num_streams;

        size_t chunk_size = strlen(data->names[i]) + 2;
        if (!is_last_name) chunk_size++;

        snprintf(size_str, 10, "%lu", chunk_size);
        write(sock, size_str, strlen(size_str));
        write(sock, "\r\n", 2);
        write(sock, "\"", 1);
        write(sock, data->names[i], strlen(data->names[i]));
        write(sock, "\"", 1);

        if (!is_last_name) {
            write(sock, ",", 1);
        }
    }
    free(size_str);

    write(sock, "1\r\n]\r\n", 6);
    write(sock, "0\r\n\r\n", 5);

    close(sock);

    mutex_lock_err = pthread_mutex_lock(&data->num_requests_lock);
    assert(mutex_lock_err == 0);

    data->num_requests--;

    if (data->num_requests == 0) {
        mutex_lock_err = pthread_mutex_lock(&data->skip_cond_lock);
        assert(mutex_lock_err == 0);

        data->skip_cond = true;

        mutex_lock_err = pthread_mutex_unlock(&data->skip_cond_lock);
        assert(mutex_lock_err == 0);

        cond_err = pthread_cond_signal(&data->names_swap_cond);
        assert(cond_err == 0);
    }

    mutex_lock_err = pthread_mutex_unlock(&data->num_requests_lock);
    assert(mutex_lock_err == 0);

    if (generate_stream_names_list) {
        unsigned int num_names = 0;
        char ** names = stream_names(map, &num_names);

        mutex_lock_err = pthread_mutex_lock(&data->skip_cond_lock);
        assert(mutex_lock_err == 0);

        if (data->skip_cond) {
            data->skip_cond = false;
        } else {
            cond_err = pthread_cond_wait(&data->names_swap_cond, &data->cond_lock);
            assert(cond_err == 0);
        }

        mutex_lock_err = pthread_mutex_unlock(&data->skip_cond_lock);
        assert(mutex_lock_err == 0);

        unsigned int old_num_names = data->num_names;
        char ** old_names = data->names;
        data->names = names;
        data->num_names = num_names;

        mutex_lock_err = pthread_mutex_unlock(&data->cond_lock);
        assert(mutex_lock_err == 0);

        for (unsigned int i = 0; i < old_num_names; i++) {
            free(old_names[i]);
        }
        if (old_names != NULL) {
            free(old_names);
        }
    }
}


void single_stream_data(
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

        char * num_subscribers_str = malloc(10);
        snprintf(num_subscribers_str, 10, "%u", num_subscribers);
        size_t str_len = strlen(num_subscribers_str);

        write(sock, HTTP_CONTENT_LENGTH, strlen(HTTP_CONTENT_LENGTH));

        char * str_len_str = malloc(10);
        snprintf(str_len_str, 10, "%lu", str_len);

        write(sock, str_len_str, strlen(str_len_str));
        write(sock, "\r\n\r\n", 4);

        write(sock, num_subscribers_str, str_len);

        free(num_subscribers_str);
        free(str_len_str);
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
        write(sock, HTTP_404, strlen(HTTP_404));
        close(sock);
    }

    close(sock);
}
