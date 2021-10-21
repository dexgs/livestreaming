#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "published_stream.h"
#include "srt/srt.h"


void increment_num_subscribers(struct published_stream_data * data, int inc) {
    int mutex_lock_err;
    // Increment num_subscribers
    mutex_lock_err = pthread_mutex_lock(&data->num_subscribers_lock);
    assert(mutex_lock_err == 0);

    data->num_subscribers += inc;

    mutex_lock_err = pthread_mutex_unlock(&data->num_subscribers_lock);
    assert(mutex_lock_err == 0);
}


struct published_stream_data * create_published_stream_data(
        SRTSOCKET sock, const char * name) 
{
    struct published_stream_data * data =
        malloc(sizeof(struct published_stream_data));

    int mutex_init_err;

    pthread_mutex_t num_subscribers_lock;
    mutex_init_err = pthread_mutex_init(&num_subscribers_lock, NULL);
    assert(mutex_init_err == 0);
    data->num_subscribers_lock = num_subscribers_lock;

    pthread_mutex_t srt_subscribers_lock;
    mutex_init_err = pthread_mutex_init(&srt_subscribers_lock, NULL);
    assert(mutex_init_err == 0);
    data->srt_subscribers_lock = srt_subscribers_lock;

    pthread_mutex_t web_subscribers_lock;
    mutex_init_err = pthread_mutex_init(&web_subscribers_lock, NULL);
    assert(mutex_init_err == 0);
    data->web_subscribers_lock = web_subscribers_lock;

    pthread_mutex_t access_lock;
    mutex_init_err = pthread_mutex_init(&access_lock, NULL);
    assert(mutex_init_err == 0);
    data->access_lock = access_lock;

    data->sock = sock;
    data->name = name;
    data->num_subscribers = 0;
    data->srt_subscribers = NULL;
    data->web_subscribers = NULL;
    return data;
}


unsigned int get_num_subscribers(struct published_stream_data * data) {
    int mutex_lock_err;
    mutex_lock_err = pthread_mutex_lock(&data->num_subscribers_lock);
    assert(mutex_lock_err == 0);

    unsigned int num_subscribers = data->num_subscribers;

    mutex_lock_err = pthread_mutex_unlock(&data->num_subscribers_lock);
    assert(mutex_lock_err == 0);

    return num_subscribers;
}


void add_srt_subscriber(struct published_stream_data * data, SRTSOCKET sock) {
    struct srt_subscriber_node * subscriber =
        malloc(sizeof(struct srt_subscriber_node));

    subscriber->sock = sock;
    subscriber->prev = NULL;

    int mutex_lock_err;

    // Add subscriber
    mutex_lock_err = pthread_mutex_lock(&data->srt_subscribers_lock);
    assert(mutex_lock_err == 0);

    subscriber->next = data->srt_subscribers;
    if (data->srt_subscribers != NULL) data->srt_subscribers->prev = subscriber;
    data->srt_subscribers = subscriber;

    mutex_lock_err = pthread_mutex_unlock(&data->srt_subscribers_lock);
    assert(mutex_lock_err == 0);

    increment_num_subscribers(data, 1);
}


// This function assumes that the current thread has locked srt_subscribers_lock
void remove_srt_subscriber_node(
        struct published_stream_data * data,
        struct srt_subscriber_node * subscriber)
{
    // Update pointers
    if (subscriber == data->srt_subscribers) data->srt_subscribers = subscriber->next;
    if (subscriber->prev != NULL) subscriber->prev->next = subscriber->next;
    if (subscriber->next != NULL) subscriber->next->prev = subscriber->prev;

    // Close socket
    srt_close(subscriber->sock);

    increment_num_subscribers(data, -1);

    // Free memory
    free(subscriber);
}


void add_web_subscriber(struct published_stream_data * data, int sock) {
    struct web_subscriber_node * subscriber =
        malloc(sizeof(struct web_subscriber_node));

    subscriber->sock = sock;
    subscriber->prev = NULL;

    int mutex_lock_err;

    // Add subscriber
    mutex_lock_err = pthread_mutex_lock(&data->web_subscribers_lock);
    assert(mutex_lock_err == 0);

    subscriber->next = data->web_subscribers;
    if (data->web_subscribers != NULL) data->web_subscribers->prev = subscriber;
    data->web_subscribers = subscriber;

    mutex_lock_err = pthread_mutex_unlock(&data->web_subscribers_lock);
    assert(mutex_lock_err == 0);

    increment_num_subscribers(data, 1);
}


void remove_web_subscriber_node(
        struct published_stream_data * data,
        struct web_subscriber_node * subscriber)
{
    // Update pointers
    if (subscriber == data->web_subscribers) data->web_subscribers = subscriber->next;
    if (subscriber->prev != NULL) subscriber->prev->next = subscriber->next;
    if (subscriber->next != NULL) subscriber->next->prev = subscriber->prev;

    // Close socket
    close(subscriber->sock);

    increment_num_subscribers(data, -1);

    // Free memory
    free(subscriber);
}


// djb2
unsigned long hash(const char * str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}


struct published_stream_node {
    struct published_stream_data * data;

    struct published_stream_node * next;
    struct published_stream_node * prev;
};


#ifndef MAP_SIZE
#define MAP_SIZE 500
#endif

struct published_stream_map {
    pthread_mutex_t map_lock;

    unsigned int max_publishers;
    unsigned int num_publishers;

    unsigned int max_subscribers_per_publisher;

    struct published_stream_node ** buckets;
};


struct published_stream_map * create_published_stream_map(
        unsigned int max_publishers,
        unsigned int max_subscribers_per_publisher)
{
    struct published_stream_map * map =
        malloc(sizeof(struct published_stream_map));

    pthread_mutex_t map_lock;
    int mutex_init_err;
    mutex_init_err = pthread_mutex_init(&map_lock, NULL);
    assert(mutex_init_err == 0);
    map->map_lock = map_lock;

    struct published_stream_node ** buckets = malloc(sizeof(void * ) * MAP_SIZE);
    for (int i = 0; i < MAP_SIZE; i++) {
        buckets[i] = NULL;
    }
    map->buckets = buckets;

    map->num_publishers = 0;
    map->max_publishers = max_publishers;
    map->max_subscribers_per_publisher = max_subscribers_per_publisher;

    return map;
}


// This function assumes map->map_lock is locked by the current thread
struct published_stream_node * get_node_with_name(
        struct published_stream_map * map, const char * name)
{
    int index = hash(name) % MAP_SIZE;

    struct published_stream_node * node = map->buckets[index];
    while (node != NULL) {
        if (strcmp(name, node->data->name) == 0) {
            break;
        }
        node = node->next;
    }

    return node;
}


bool max_publishers_exceeded(struct published_stream_map * map) {
    int mutex_lock_err;

    mutex_lock_err = pthread_mutex_lock(&map->map_lock);
    assert(mutex_lock_err == 0);

    bool max_publishers_exceeded =
        map->max_publishers > 0 && map->num_publishers >= map->max_publishers;

    mutex_lock_err = pthread_mutex_unlock(&map->map_lock);
    assert(mutex_lock_err == 0);

    return max_publishers_exceeded;
}


bool stream_name_in_map(struct published_stream_map * map, const char * name) {
    int mutex_lock_err;

    mutex_lock_err = pthread_mutex_lock(&map->map_lock);
    assert(mutex_lock_err == 0);

    bool stream_name_in_map = get_node_with_name(map, name) != NULL;

    mutex_lock_err = pthread_mutex_unlock(&map->map_lock);
    assert(mutex_lock_err == 0);

    return stream_name_in_map;
}


char ** stream_names(struct published_stream_map * map, int * num_streams) {
    int mutex_lock_err;

    mutex_lock_err = pthread_mutex_lock(&map->map_lock);
    assert(mutex_lock_err == 0);

    char ** stream_names = malloc(sizeof(char *) * map->num_publishers);
    *num_streams = map->num_publishers;

    unsigned int stream_names_index = 0;

    for (int i = 0; i < MAP_SIZE; i++) {
        struct published_stream_node * node = map->buckets[i];

        while (node != NULL) {
            char * name = malloc(strlen(node->data->name));
            name = strcpy(name, node->data->name);

            assert(stream_names_index < map->num_publishers);
            stream_names[stream_names_index] = name;
            stream_names_index++;

            node = node->next;
        }
    }

    mutex_lock_err = pthread_mutex_unlock(&map->map_lock);
    assert(mutex_lock_err == 0);

    return stream_names;
}


struct published_stream_data * add_stream_to_map(
        struct published_stream_map * map, SRTSOCKET sock, const char * name)
{
    int mutex_lock_err;

    mutex_lock_err = pthread_mutex_lock(&map->map_lock);
    assert(mutex_lock_err == 0);

    struct published_stream_data * data =
        create_published_stream_data(sock, name);

    int index = hash(name) % MAP_SIZE;

    struct published_stream_node * new_node =
        malloc(sizeof(struct published_stream_node));

    new_node->data = data;
    new_node->prev = NULL;

    new_node->next = map->buckets[index];
    if (map->buckets[index] != NULL) map->buckets[index]->prev = new_node;
    map->buckets[index] = new_node;

    map->num_publishers++;

    mutex_lock_err = pthread_mutex_unlock(&map->map_lock);
    assert(mutex_lock_err == 0);

    return data;
}


void remove_stream_from_map(
        struct published_stream_map * map, const char * name)
{
    int mutex_lock_err;

    mutex_lock_err = pthread_mutex_lock(&map->map_lock);
    assert(mutex_lock_err == 0);

    int index = hash(name) % MAP_SIZE;

    struct published_stream_node * node = map->buckets[index];
    while (node != NULL) {
        if (strcmp(name, node->data->name) == 0) {
            break;
        }
        node = node->next;
    }

    if (node != NULL) {
        if (node == map->buckets[index]) map->buckets[index] = node->next;
        if (node->prev != NULL) node->prev->next = node->next;
        if (node->next != NULL) node->next->prev = node->prev;
        free(node);
    }

    map->num_publishers--;

    mutex_lock_err = pthread_mutex_unlock(&map->map_lock);
    assert(mutex_lock_err == 0);
}


struct published_stream_data * get_stream_from_map(
        struct published_stream_map * map, const char * name)
{
    int mutex_lock_err;

    mutex_lock_err = pthread_mutex_lock(&map->map_lock);
    assert(mutex_lock_err == 0);

    struct published_stream_data * data = NULL;

    struct published_stream_node * node = get_node_with_name(map, name);
    if (node != NULL) data = node->data;

    mutex_lock_err = pthread_mutex_lock(&data->access_lock);
    if (mutex_lock_err != 0) data = NULL;

    mutex_lock_err = pthread_mutex_unlock(&map->map_lock);
    assert(mutex_lock_err == 0);

    return data;
}

bool max_subscribers_exceeded(
        struct published_stream_map * map, const char * name)
{
    int mutex_lock_err;

    mutex_lock_err = pthread_mutex_lock(&map->map_lock);
    assert(mutex_lock_err == 0);

    bool max_subscribers_exceeded = false;

    struct published_stream_node * node = get_node_with_name(map, name);

    if (node != NULL) {
        struct published_stream_data * data = node->data;
        mutex_lock_err = pthread_mutex_lock(&data->num_subscribers_lock);
        assert(mutex_lock_err == 0);

        max_subscribers_exceeded =
            map->max_subscribers_per_publisher > 0
            && data->num_subscribers >= map->max_subscribers_per_publisher;

        mutex_lock_err = pthread_mutex_unlock(&data->num_subscribers_lock);
        assert(mutex_lock_err == 0);
    }

    mutex_lock_err = pthread_mutex_unlock(&map->map_lock);
    assert(mutex_lock_err == 0);

    return max_subscribers_exceeded;
}
