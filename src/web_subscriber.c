#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include "web_subscriber.h"
#include "authenticator.h"
#include "published_stream.h"
#include "picohttpparser/picohttpparser.h"

struct thread_data {
    int sock;
    bool read_web_ip_from_headers;
    char * addr;
    struct authenticator * auth;
    struct published_stream_map * map;
};

void * run_web_subscriber(void * _d);

void web_subscriber(
        int sock, bool read_web_ip_from_headers, char * addr,
        struct authenticator * auth, struct published_stream_map * map)
{
    struct thread_data * d = malloc(sizeof(struct thread_data));
    d->sock = sock;
    d->read_web_ip_from_headers = read_web_ip_from_headers;
    d->addr = addr;
    d->auth = auth;
    d->map = map;

    pthread_t thread_handle;
    int thread_err = pthread_create(&thread_handle, NULL, run_web_subscriber, d);
    assert(thread_err == 0);
}

// Return heap allocated string with contents equal to str with `prefix` and
// trailing slash (if present) removed.
char * strip_prefix(const char * prefix, const char * str, size_t str_len) {
    size_t prefix_len = strlen(prefix);
    char * stripped;
    size_t stripped_len;
    // Handle trailing slash
    if (str[str_len - 1] == '/') {
        stripped_len = str_len - prefix_len - 1;
    } else {
        stripped_len = str_len - prefix_len;
    }
    // Copy into heap allocated string
    stripped = malloc(stripped_len + 1);
    stripped = strncpy(stripped, str + prefix_len, stripped_len);
    stripped[stripped_len] = '\0';
    return stripped;
}

void * run_web_subscriber(void * _d) {
    struct thread_data * d = (struct thread_data *) _d;
    int sock = d->sock;
    bool read_web_ip_from_headers = d->read_web_ip_from_headers;
    char * addr = d->addr;
    struct authenticator * auth = d->auth;
    struct published_stream_map * map = d->map;
    free(d);

    char buf[1024];
    ssize_t bytes_read = read(sock, buf, sizeof(buf));
    if (bytes_read < 0) {
        free(addr);
        return NULL;
    }

    // HTTP request data
    const char * method;
    size_t method_len;
    const char * path;
    size_t path_len;
    int minor_version;
    struct phr_header headers[100];
    size_t num_headers = 100;

    int parse_err = phr_parse_request(
            buf, sizeof(buf), &method, &method_len, &path,
            &path_len,&minor_version, headers, &num_headers, 0);

    if (parse_err > 0) {
        if (path_len > 8 && strncmp("/stream/", path, 8) == 0) {
            // /stream/
            char * name = strip_prefix("/stream/", path, path_len);

            if (read_web_ip_from_headers) {
                // Parse IP from X-Forwarded-For header
                for (size_t i = 0; i < num_headers; i++) {
                    if (strncmp(
                                "X-Forwarded-For", headers[i].name,
                                headers[i].name_len) == 0) 
                    {
                        char * header_addr = 
                            malloc(headers[i].value_len + 1);
                        header_addr = strncpy(
                                header_addr, headers[i].value,
                                headers[i].value_len);
                        header_addr[headers[i].value_len] = '\0';
                        free(addr);
                        addr = header_addr;
                        break;
                    }
                }
            }

            char * processed_name = authenticate(auth, false, addr, name);
            free(name);
            free(addr);
            name = processed_name;

            // Close connection prematurely if...
            if (
                    // If authentication failed
                    name == NULL
                    // If there is no active stream with the requested name
                    || !stream_name_in_map(map, name)
                    // If the maximum number of subscribers to the stream would
                    // be exceeded
                    || max_subscribers_exceeded(map, name))
            {
                close(sock);
                return NULL;
            }

            struct published_stream_data * data = get_stream_from_map(map, name);
            if (data != NULL) {
                char * response = 
                    "HTTP/1.1 200 OK\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Content-Type: video/mp2t\r\n"
                    "Transfer-Encoding: chunked\r\n\r\n";
                write(sock, response, strlen(response));
                add_web_subscriber(data, sock);
                int mutex_lock_err = pthread_mutex_unlock(&data->access_lock);
                assert(mutex_lock_err == 0);
            } else {
                close(sock);
            }
        } else if (path_len > 5 && strncmp("/api/", path, 5) == 0) {
            free(addr);
            // /api/ TODO: implement this later
            close(sock);
        } else {
            free(addr);
            close(sock);
        }
    }

    return NULL;
}

int write_to_web_subscriber(
        int sock, char * hex, unsigned int hex_len,
        char * data, unsigned int data_len)
{
    char newline[2] = {'\r', '\n'};
    char * buffers[4] = {hex, newline, data, newline};
    unsigned int lengths[4] = {hex_len, 2, data_len, 2};

    int total_bytes = 0;
    int bytes_written;
    for (int i = 0; i < 4; i++) {
        if (lengths[i] > 0) {
            bytes_written = write(sock, buffers[i], lengths[i]);
            if (bytes_written < 0) return bytes_written;
            total_bytes += bytes_written;
        }
    }

    return total_bytes;
}

void get_len_hex(
        unsigned int data_len, char * buf, unsigned int buf_len,
        unsigned int * hex_len)
{
    if (data_len == 0) {
        *hex_len = 1;
        snprintf(buf, buf_len, "%x", 0);
    } else {
        *hex_len = (log(data_len) / log(16)) + 1;
        snprintf(buf, buf_len, "%x", data_len);
    }
}