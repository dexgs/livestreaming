#include <assert.h>
#include <stdio.h>

#include "guard.h"
#include "srt_common.h"
#include "srt_publisher.h"
#include "web_subscriber.h"


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

#ifndef NDEBUG
    printf("`%s` started publishing `%s`\n", addr, name);
#endif

    char buf[SRT_BUFFER_SIZE] = {0};
    int bytes_read = 0;
    int inorder = 0;

    char hex[12] = {0};

    while (bytes_read != SRT_ERROR) {
        int send_err;

        // Send data to SRT subscribers
        GUARD(&data->srt_subscribers_lock, {
            struct srt_subscriber_node * srt_node = data->srt_subscribers;
            while (srt_node != NULL) {
                send_err = srt_sendmsg(srt_node->sock, buf, bytes_read, -1, inorder);
                struct srt_subscriber_node * next_node = srt_node->next;

                // If sending failed, remove the subscriber
                if (send_err == SRT_ERROR) {
                    remove_srt_subscriber_node(data, srt_node);
                }

                srt_node = next_node;
            }
        })

        // Send data to Web subscribers
        GUARD(&data->web_subscribers_lock, {
            unsigned int hex_len;
            get_len_hex(bytes_read, hex, sizeof(hex), &hex_len);

            struct web_subscriber_node * web_node = data->web_subscribers;
            while (web_node != NULL) {
                send_err = write_to_web_subscriber(
                        web_node->sock, hex, hex_len, buf, bytes_read);

                struct web_subscriber_node * next_node = web_node->next;

                if (send_err == -1) {
                    // Allow up to MAX_WEB_SEND_FAILS EAGAIN/EWOULDBLOCK errors
                    // before removing the web node. Immediatly remove the web
                    // node on any other error type.
                    if (
                            web_node->num_fails <= MAX_WEB_SEND_FAILS
                            && (errno == EAGAIN || errno == EWOULDBLOCK))
                    {
                        web_node->num_fails++;
                    } else {
                        remove_web_subscriber_node(data, web_node);
                    }
                }

                web_node = next_node;
            }
        })

        // Fill buffer with new data
        SRT_MSGCTRL mctrl;
        bytes_read = srt_recvmsg2(sock, buf, SRT_BUFFER_SIZE, &mctrl);
        inorder = mctrl.inorder;
    }

#ifndef NDEBUG
    printf("`%s` stopped publishing `%s`\n", addr, name);
#endif
    free(addr);

    remove_stream_from_map(map, data);

    return NULL;
}
