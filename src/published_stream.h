#ifndef PUBLISHED_STREAM_H_
#define PUBLISHED_STREAM_H_

#include <stdbool.h>
#include <pthread.h>
#include "thirdparty/srt/srt.h"

struct published_stream_data {
    SRTSOCKET sock;
    const char * name;

    pthread_mutex_t num_subscribers_lock;
    unsigned int num_subscribers;

    pthread_mutex_t srt_subscribers_lock;
    struct srt_subscriber_node * srt_subscribers;

    pthread_mutex_t webrtc_subscribers_lock;
    struct webrtc_subscriber_node * webrtc_subscribers;
};

// called from outside the thread for a stream
unsigned int get_num_subscribers(struct published_stream_data * data);

// called from outside the thread for a stream
void add_srt_subscriber(struct published_stream_data * data, SRTSOCKET sock);
// called from inside the thread for a stream if sending data failed
// its implementation assumes srt_subscribers_lock has been acquired
void remove_srt_subscriber_node(
        struct published_stream_data * data,
        struct srt_subscriber_node * subscriber);

// called from outside the thread for a stream
void add_webrtc_subscriber(struct published_stream_data * data);
// called from inside the thread for a stream if sending data failed
// its implementation assumes webrtc_subscribers_lock has been acquired
void remove_webrtc_subscriber_node(
        struct published_stream_data * data,
        struct webrtc_subscriber_node * subscriber);



struct srt_subscriber_node {
    SRTSOCKET sock;
    struct srt_subscriber_node * next;
    struct srt_subscriber_node * prev;
};

struct webrtc_subscriber_node {
    struct webrtc_subscriber_node * next;
    struct webrtc_subscriber_node * prev;
};



struct published_stream_map;

struct published_stream_map * create_published_stream_map(
        unsigned int max_publishers,
        unsigned int max_subscribers_per_publisher);

bool max_publishers_exceeded(struct published_stream_map * map);

bool stream_name_in_map(struct published_stream_map * map, const char * name);

char ** stream_names(struct published_stream_map * map);

void add_stream_to_map(
        struct published_stream_map * map, SRTSOCKET sock, const char * name);

void remove_stream_from_map(
        struct published_stream_map * map, const char * name);

struct published_stream_data * get_stream_from_map(
        struct published_stream_map * map, const char * name);

bool max_subscribers_exceeded(
        struct published_stream_map * map, const char * name);

#endif
