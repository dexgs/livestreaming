#ifndef PUBLISHED_STREAM_H_
#define PUBLISHED_STREAM_H_

#include <stdbool.h>
#include <pthread.h>
#include "srt/srt.h"

// Represents the data related to a single livestream.
// It encapsulates the socket over which incoming data is received
// as well ass access to the connections over which the stream is
// broadcast (along with the locks required for safe use across threads).
struct published_stream_data {
    SRTSOCKET sock;
    const char * name;

    pthread_mutex_t num_subscribers_lock;
    unsigned int num_subscribers;

    pthread_mutex_t srt_subscribers_lock;
    struct srt_subscriber_node * srt_subscribers;

    pthread_mutex_t web_subscribers_lock;
    struct web_subscriber_node * web_subscribers;

    pthread_mutex_t access_lock;
};

// called from outside the thread for a stream
unsigned int get_num_subscribers(struct published_stream_data * data);

// called from outside the thread for a stream
void add_srt_subscriber(struct published_stream_data * data, SRTSOCKET sock);
// called from inside the thread for a stream if sending data failed
// its implementation assumes `srt_subscribers_lock` has been acquired
// and will close the socket of the node that was removed.
void remove_srt_subscriber_node(
        struct published_stream_data * data,
        struct srt_subscriber_node * subscriber);

// called from outside the thread for a stream
void add_web_subscriber(struct published_stream_data * data, int sock);
// called from inside the thread for a stream if sending data failed
// its implementation assumes `web_subscribers_lock` has been acquired
// and will close the socket of the node that was removed.
void remove_web_subscriber_node(
        struct published_stream_data * data,
        struct web_subscriber_node * subscriber);


struct srt_subscriber_node {
    SRTSOCKET sock;
    struct srt_subscriber_node * next;
    struct srt_subscriber_node * prev;
};

struct web_subscriber_node {
    int sock;
    struct web_subscriber_node * next;
    struct web_subscriber_node * prev;
};



// Represents the mapping between stream names and their respective
// `published_stream_data` instances.
struct published_stream_map;

struct published_stream_map * create_published_stream_map(
        unsigned int max_publishers,
        unsigned int max_subscribers_per_publisher);

bool max_publishers_exceeded(struct published_stream_map * map);

bool stream_name_in_map(struct published_stream_map * map, const char * name);

// All names are copied into the returned string array. There will be no memory
// shared between the returned strings and the contents of the map. Because of
// this, num_streams *must* be used as the length for iterating over the names
// because the size of the array may go out of sync with the contents of the map.
char ** stream_names(struct published_stream_map * map, int * num_streams);

struct published_stream_data * add_stream_to_map(
        struct published_stream_map * map, SRTSOCKET sock, const char * name);

// This function does not free the published_stream_data with the given name
void remove_stream_from_map(
        struct published_stream_map * map, const char * name);

// This function locks the `acces_lock` member of the returned data. It must be
// properly unlocked by the caller.
struct published_stream_data * get_stream_from_map(
        struct published_stream_map * map, const char * name);

bool max_subscribers_exceeded(
        struct published_stream_map * map, const char * name);

#endif
