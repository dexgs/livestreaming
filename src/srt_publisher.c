#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include "srt_publisher.h"
#include "srt/srt.h"
#include "published_stream.h"
#include "web_subscriber.h"
#include "authenticator.h"
#include "srt_common.h"


void * srt_publisher(void * _d) {
    struct srt_thread_data * d = (struct srt_thread_data *) _d;
    SRTSOCKET sock = d->sock;
    char * addr = d->addr;
    struct authenticator * auth = d->auth;
    struct published_stream_map * map = d->map;
    free(d);

    char * name = get_socket_stream_id(sock);

    // Add the stream and acquire the created stream data
    struct published_stream_data * data =
        add_stream_to_map(map, auth, &name, addr, sock);
    if (data == NULL) return NULL;

    printf("`%s` started publishing `%s`\n", addr, name);

    char buf[SRT_BUFFER_SIZE];
    int mutex_lock_err;
    int bytes_read = 0;

    char hex[12];

    while (bytes_read != SRT_ERROR) {
        int send_err;

        // Send data to SRT subscribers
        mutex_lock_err = pthread_mutex_lock(&data->srt_subscribers_lock);
        assert(mutex_lock_err == 0);

        struct srt_subscriber_node * srt_node = data->srt_subscribers;
        while (srt_node != NULL) {
            send_err = srt_sendmsg(srt_node->sock, buf, bytes_read, 0, false);
            struct srt_subscriber_node * next_node = srt_node->next;

            // If sending failed, remove the subscriber
            if (send_err == SRT_ERROR) {
                remove_srt_subscriber_node(data, srt_node);
            }

            srt_node = next_node;
        }

        mutex_lock_err = pthread_mutex_unlock(&data->srt_subscribers_lock);
        assert(mutex_lock_err == 0);

        // Send data to Web subscribers
        mutex_lock_err = pthread_mutex_lock(&data->web_subscribers_lock);
        assert(mutex_lock_err == 0);

        unsigned int hex_len;
        get_len_hex(bytes_read, hex, sizeof(hex), &hex_len);

        struct web_subscriber_node * web_node = data->web_subscribers;
        while (web_node != NULL) {
            send_err = write_to_web_subscriber(
                    web_node->sock, hex, hex_len, buf, bytes_read);

            struct web_subscriber_node * next_node = web_node->next;

            // If sending failed, remove the subscriber
            if (send_err == -1) {
                remove_web_subscriber_node(data, web_node);
            }

            web_node = next_node;
        }

        mutex_lock_err = pthread_mutex_unlock(&data->web_subscribers_lock);
        assert(mutex_lock_err == 0);

        // Fill buffer with new data
        bytes_read = srt_recv(sock, buf, SRT_BUFFER_SIZE);
    }

    printf("`%s` stopped publishing `%s`\n", addr, name);
    free(addr);

    remove_stream_from_map(map, data);

    return NULL;
}
