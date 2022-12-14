#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include "web_subscriber.h"
#include "web_api.h"
#include "authenticator.h"
#include "published_stream.h"
#include "picohttpparser.h"

struct thread_data {
    int sock;
    bool read_web_ip_from_headers;
    char * addr;
    struct authenticator * auth;
    struct published_stream_map * map;
    struct web_api_data * data;
};

void * run_web_subscriber(void * _d);

void web_subscriber(
        int sock, bool read_web_ip_from_headers, char * addr,
        struct authenticator * auth, struct published_stream_map * map,
        struct web_api_data * data)
{
    struct thread_data * d = malloc(sizeof(struct thread_data));
    d->sock = sock;
    d->read_web_ip_from_headers = read_web_ip_from_headers;
    d->addr = addr;
    d->auth = auth;
    d->map = map;
    d->data = data;

    pthread_t thread_handle;
    int pthread_err; 

    pthread_err = pthread_create(&thread_handle, NULL, run_web_subscriber, d);
    assert(pthread_err == 0);

    pthread_err = pthread_detach(thread_handle);
    assert(pthread_err == 0);
}

// Return string with contents equal to str with `prefix` and trailing slash
// (if present) removed. The returned string is backed by the same memory as
// `str` and trimming any trailing slash mutates str.
char * strip_prefix(const char * prefix, char * str, size_t str_len) {
    size_t prefix_len = strlen(prefix);
    char * stripped;
    size_t stripped_len;
    // Handle trailing slash
    if (str[str_len - 1] == '/') {
        stripped_len = str_len - prefix_len - 1;
    } else {
        stripped_len = str_len - prefix_len;
    }
    stripped = str + prefix_len;
    stripped[stripped_len] = '\0';
    return stripped;
}


const char * STREAM_PATH = "/stream/";
const char * API_PATH = "/api/";
const char * DOUBLE_CR = "\r\n\r\n";

void * run_web_subscriber(void * _d) {
    struct thread_data * d = (struct thread_data *) _d;
    int sock = d->sock;
    bool read_web_ip_from_headers = d->read_web_ip_from_headers;
    char * addr = d->addr;
    struct authenticator * auth = d->auth;
    struct published_stream_map * map = d->map;
    struct web_api_data * data = d->data;
    free(d);

    char buf[1024] = {0};
    // "- 1" is significant. It's here so that we can make any substring of
    // `buf` null terminated without writing to out-of-bounds memory.
    size_t buf_size = sizeof(buf) - 1;
    size_t bytes_read = 0;
    while (bytes_read < buf_size) {
        ssize_t b = read(sock, buf + bytes_read, buf_size - bytes_read);

        if (b > 0) {
            bytes_read += b;
        }

        if (strncmp(DOUBLE_CR, buf + bytes_read - 4, 4) == 0) {
            break;
        } else if (b < 256) {
            close(sock);
            free(addr);
            return NULL;
        }
    }

    // HTTP request data
    const char * method;
    size_t method_len;
    char * path;
    size_t path_len;
    int minor_version;
    struct phr_header headers[100];
    size_t num_headers = 100;

    int parse_err = phr_parse_request(
            buf, buf_size, &method, &method_len, (const char **) &path,
            &path_len, &minor_version, headers, &num_headers, 0);

    if (parse_err <= 0) {
        free(addr);
        close(sock);
        return NULL;
    }

    if (
            strlen(STREAM_PATH) <= path_len
            && strncmp(STREAM_PATH, path, strlen(STREAM_PATH)) == 0) 
    {
        // /stream/
        char * name;
        if (path_len == 8) {
            name = malloc(1);
            name[0] = '\0';
        } else {
            name = strdup(strip_prefix(STREAM_PATH, path, path_len));
        }

        if (read_web_ip_from_headers) {
            // Parse IP from "X-Real-IP" header
            for (size_t i = 0; i < num_headers; i++) {
                if (strncmp(
                            "X-Real-IP", headers[i].name,
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

        add_web_subscriber(map, auth, name, addr, sock);
    } else {
        free(addr);
        if (
                path_len > strlen(API_PATH)
                && strncmp(API_PATH, path, strlen(API_PATH)) == 0)
        {
            path = strip_prefix("/api/", path, path_len);
            web_api(map, data, path, path_len - strlen(API_PATH), sock);
        } else {
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

    int on = 1;
    setsockopt(sock, SOL_TCP, TCP_CORK, &on, sizeof(on));

    for (int i = 0; i < 4; i++) {
        if (lengths[i] > 0) {
            bytes_written = write(sock, buffers[i], lengths[i]);
            if (bytes_written < 0) return bytes_written;
            total_bytes += bytes_written;
        }
    }

    int off = 0;
    setsockopt(sock, SOL_TCP, TCP_CORK, &off, sizeof(off));

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
