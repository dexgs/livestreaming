#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include "srt_publisher.h"
#include "thirdparty/srt/srt.h"
#include "published_stream.h"
#include "authenticator.h"
#include "srt_common.h"


#ifndef SRT_BUFFER_SIZE
#define SRT_BUFFER_SIZE 4096
#endif

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
        srt_close(sock);
        return NULL;
    }

    printf("`%s` started publishing `%s`\n", addr, name);

    // Add the stream and acquire the created stream data
    add_stream_to_map(map, sock, name);
    struct published_stream_data * data =
        get_stream_from_map(map, name);
    assert(data != NULL);

    char buf[SRT_BUFFER_SIZE];
    int recv_err = 0;

    int mutex_lock_err;

    while (recv_err != SRT_ERROR) {
        int send_err;

        // Send data to SRT subscribers
        mutex_lock_err = pthread_mutex_lock(&data->srt_subscribers_lock);
        assert(mutex_lock_err == 0);

        struct srt_subscriber_node * srt_node = data->srt_subscribers;
        while (srt_node != NULL) {
            send_err = srt_send(srt_node->sock, buf, SRT_BUFFER_SIZE);
            struct srt_subscriber_node * next_node = srt_node->next;

            // If sending failed, remove the subscriber
            if (send_err != 0) {
                remove_srt_subscriber_node(data, srt_node);
            }

            srt_node = next_node;
        }

        mutex_lock_err = pthread_mutex_unlock(&data->srt_subscribers_lock);
        assert(mutex_lock_err == 0);

        // Send data to WebRTC subscribers
        mutex_lock_err = pthread_mutex_lock(&data->webrtc_subscribers_lock);
        assert(mutex_lock_err == 0);

        struct webrtc_subscriber_node * webrtc_node = data->webrtc_subscribers;
        while (webrtc_node != NULL) {
            send_err = 0; // TODO: WebRTC sending goes here
            struct webrtc_subscriber_node * next_node = webrtc_node->next;

            // If sending failed, remove the subscriber
            if (send_err != 0) {
                // remove_webrtc_subscriber_node(data, webrtc_node);
            }

            webrtc_node = next_node;
        }

        mutex_lock_err = pthread_mutex_unlock(&data->srt_subscribers_lock);
        assert(mutex_lock_err == 0);

        // Fill buffer with new data
        recv_err = srt_recv(sock, buf, SRT_BUFFER_SIZE);
    }

    srt_close(sock);
    printf("`%s` stopped publishing `%s`\n", addr, name);

    remove_stream_from_map(map, name);
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
    // Clean up WebRTC subscribers
    // Free the published_stream_data
    free(data);

    return NULL;
}
