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

    char * processed_name = authenticate(auth, true, addr, name);
    free(name);
    name = processed_name;

    // Close the connection prematurely if...
    if (
            // If authentication failed
            name == NULL
            // If no more publishers are allowed
            || max_publishers_exceeded(map)
            // If another stream is already using the chosen name
            || stream_name_in_map(map, name)) 
    {
        goto close;
    }

    printf("`%s` started publishing `%s`\n", addr, name);

    // Add the stream and acquire the created stream data
    struct published_stream_data * data = add_stream_to_map(map, sock, name);
    if (data == NULL) {
        goto close;
    }

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
            send_err = srt_send(srt_node->sock, buf, bytes_read);
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

    srt_close(sock);
    printf("`%s` stopped publishing `%s`\n", addr, name);

    mutex_lock_err = pthread_mutex_lock(&data->access_lock);
    assert(mutex_lock_err == 0);

    remove_stream_from_map(map, name);

    mutex_lock_err = pthread_mutex_unlock(&data->access_lock);
    assert(mutex_lock_err == 0);

    free(name);
    free(addr);

    // Clean up SRT subscribers
    mutex_lock_err = pthread_mutex_lock(&data->srt_subscribers_lock);
    assert(mutex_lock_err == 0);

    struct srt_subscriber_node * srt_node = data->srt_subscribers;
    while (srt_node != NULL) {
        srt_close(srt_node->sock);
        struct srt_subscriber_node * next_node = srt_node->next;
        free(srt_node);
        srt_node = next_node;
    }

    mutex_lock_err = pthread_mutex_unlock(&data->srt_subscribers_lock);
    assert(mutex_lock_err == 0);

    // Clean up Web subscribers
    mutex_lock_err = pthread_mutex_lock(&data->web_subscribers_lock);
    assert(mutex_lock_err == 0);

    struct web_subscriber_node * web_node = data->web_subscribers;
    while (web_node != NULL) {
        write(web_node->sock, "0\r\n\r\n", 5);
        close(web_node->sock);
        struct web_subscriber_node * next_node = web_node->next;
        free(web_node);
        web_node = next_node;
    }

    mutex_lock_err = pthread_mutex_unlock(&data->web_subscribers_lock);
    assert(mutex_lock_err == 0);

    // Free the published_stream_data
    pthread_mutex_destroy(&data->num_subscribers_lock);
    pthread_mutex_destroy(&data->srt_subscribers_lock);
    pthread_mutex_destroy(&data->web_subscribers_lock);
    pthread_mutex_destroy(&data->access_lock);
    free(data);

    return NULL;

close:
    if (name != NULL) free(name);
    if (addr != NULL) free(addr);
    if (data != NULL) free(data);
    srt_close(sock);
    return NULL;
}
