#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h> 
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "published_stream.h"
#include "authenticator.h"
#include "srt.h"


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
        SRTSOCKET sock, char * name)
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


void add_srt_subscriber_to_stream(
        struct published_stream_data * data, SRTSOCKET sock) 
{
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


void add_web_subscriber_to_stream(
        struct published_stream_data * data, int sock)
{
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


struct published_stream_map {
    pthread_mutex_t map_lock;

    unsigned int max_publishers;
    unsigned int num_publishers;

    unsigned int max_subscribers_per_publisher;

    struct published_stream_node * buckets[MAP_SIZE];
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

    for (int i = 0; i < MAP_SIZE; i++) {
        map->buckets[i] = NULL;
    }

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


unsigned int num_streams(struct published_stream_map * map) {
    int mutex_lock_err;

    mutex_lock_err = pthread_mutex_lock(&map->map_lock);
    assert(mutex_lock_err == 0);

    unsigned int num_streams = map->num_publishers;

    mutex_lock_err = pthread_mutex_unlock(&map->map_lock);
    assert(mutex_lock_err == 0);

    return num_streams;
}


struct stream_info {
    char * name;
    unsigned int num_subscribers;
};

// Using Hoare's partition scheme
unsigned int partition_stream_info(
        struct stream_info ** streams, unsigned int len, unsigned int pivot)
{
    struct stream_info * pivot_info = streams[pivot];
    int l = -1;
    int h = len;

    while (true) {
        do {
            l++;
        } while (streams[l]->num_subscribers > pivot_info->num_subscribers);

        do {
            h--;
        } while (streams[h]->num_subscribers < pivot_info->num_subscribers);

        if (l >= h) {
            return l;
        }
        // swap
        struct stream_info * temp = streams[l];
        streams[l] = streams[h];
        streams[h] = temp;
    }
}

void sort_stream_info(
        struct stream_info ** streams, unsigned int len)
{
    if (len >= 2) {
        unsigned int pivot = len / 2;
        pivot = partition_stream_info(streams, len, pivot);
        sort_stream_info(streams, pivot);
        sort_stream_info(streams + pivot, len - pivot);
    }
}

// Return list of stream names sorted by number of watchers
char ** stream_names(
        struct published_stream_map * map, unsigned int * num_streams)
{
    int mutex_lock_err;

    mutex_lock_err = pthread_mutex_lock(&map->map_lock);
    assert(mutex_lock_err == 0);

    unsigned int streams_len = map->num_publishers;

    mutex_lock_err = pthread_mutex_unlock(&map->map_lock);
    assert(mutex_lock_err == 0);

    if (streams_len == 0) {
        *num_streams = 0;
        return NULL;
    }

    struct stream_info ** streams =
        malloc(sizeof(struct stream_info *) * streams_len);

    unsigned int streams_index = 0;

    // Collect list of stream info
    for (int i = 0; i < MAP_SIZE; i++) {
        mutex_lock_err = pthread_mutex_lock(&map->map_lock);
        assert(mutex_lock_err == 0);

        struct published_stream_node * node = map->buckets[i];

        while (node != NULL) {
            // If stream is not unlisted
            if (!(
                        strlen(node->data->name) >= strlen(UNLISTED_STREAM_NAME_PREFIX)
                        && strncmp(
                            UNLISTED_STREAM_NAME_PREFIX, node->data->name,
                            strlen(UNLISTED_STREAM_NAME_PREFIX)) == 0))
            {
                struct stream_info * info = malloc(sizeof(struct stream_info));

                info->name = strdup(node->data->name);

                mutex_lock_err =
                    pthread_mutex_lock(&node->data->num_subscribers_lock);
                assert (mutex_lock_err == 0);

                info->num_subscribers = node->data->num_subscribers;

                mutex_lock_err =
                    pthread_mutex_unlock(&node->data->num_subscribers_lock);
                assert (mutex_lock_err == 0);

                // If there isn't enough room to add another stream
                if (streams_index >= streams_len) {
                    streams_len *= 2;
                    streams =
                        realloc(streams, sizeof(struct stream_info *) * streams_len);
                }

                streams[streams_index] = info;
                streams_index++;
            }
            node = node->next;
        }

        mutex_lock_err = pthread_mutex_unlock(&map->map_lock);
        assert(mutex_lock_err == 0);
    }

    if (streams_index == 0) {
        free(streams);
        return NULL;
    }

    streams = realloc(streams, sizeof(struct stream_info *) * streams_index);

    // Sort stream info by number of subscribers (quicksort)
    sort_stream_info(streams, streams_index);

    char ** stream_names = malloc(sizeof(char *) * streams_index);
    for(unsigned int i = 0; i < streams_index; i++) {
        stream_names[i] = streams[i]->name;
        free(streams[i]);
    }
    free(streams);

    *num_streams = streams_index;
    return stream_names;
}


struct published_stream_data * create_stream_data_in_map(
        struct published_stream_map * map, SRTSOCKET sock, char * name)
{
    int mutex_lock_err;

    mutex_lock_err = pthread_mutex_lock(&map->map_lock);
    assert(mutex_lock_err == 0);

    struct published_stream_data * data;

    bool max_publishers_exceeded =
        map->max_publishers > 0 && map->num_publishers >= map->max_publishers;
    bool stream_name_in_map = get_node_with_name(map, name) != NULL;

    if (max_publishers_exceeded || stream_name_in_map) {
        data = NULL;
    } else {
        data = create_published_stream_data(sock, name);

        int index = hash(name) % MAP_SIZE;

        struct published_stream_node * new_node =
            malloc(sizeof(struct published_stream_node));

        new_node->data = data;
        new_node->prev = NULL;

        new_node->next = map->buckets[index];
        if (map->buckets[index] != NULL) map->buckets[index]->prev = new_node;
        map->buckets[index] = new_node;

        map->num_publishers++;
    }

    mutex_lock_err = pthread_mutex_unlock(&map->map_lock);
    assert(mutex_lock_err == 0);

    return data;
}


struct published_stream_data * add_stream_to_map(
        struct published_stream_map * map, struct authenticator * auth,
        char ** name, char * addr, SRTSOCKET sock)
{
    char * processed_name = authenticate(auth, true, addr, *name);
    free(*name);
    *name = processed_name;

    // If authentication failed
    if (*name == NULL) {
        srt_close(sock);
        return NULL;
    }

    struct published_stream_data * data =
        create_stream_data_in_map(map, sock, *name);
    if (data == NULL) {
        free(*name);
        free(addr);
        srt_close(sock);
    }

    return data;
}


void remove_name_from_map(
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
        mutex_lock_err = pthread_mutex_lock(&node->data->access_lock);
        assert(mutex_lock_err == 0);

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
    if (node != NULL) {
        data = node->data;
        mutex_lock_err = pthread_mutex_lock(&data->access_lock);
        if (mutex_lock_err != 0) data = NULL;
    }

    mutex_lock_err = pthread_mutex_unlock(&map->map_lock);
    assert(mutex_lock_err == 0);

    return data;
}

bool max_subscribers_exceeded(
        struct published_stream_map * map, struct published_stream_data * data)
{
    int mutex_lock_err;

    mutex_lock_err = pthread_mutex_lock(&data->num_subscribers_lock);
    assert(mutex_lock_err == 0);

    bool max_subscribers_exceeded =
        map->max_subscribers_per_publisher > 0
        && data->num_subscribers >= map->max_subscribers_per_publisher;

    mutex_lock_err = pthread_mutex_unlock(&data->num_subscribers_lock);
    assert(mutex_lock_err == 0);

    return max_subscribers_exceeded;
}

// Common setup between adding an SRT subscriber and adding a web subscriber
struct published_stream_data * add_subscriber_common(
        struct published_stream_map * map,
        struct authenticator * auth, char * name, char * addr)
{
    char * processed_name = authenticate(auth, false, addr, name);
    free(name);
    free(addr);
    name = processed_name;

    // If authentication failed
    if (name == NULL) return NULL;

    struct published_stream_data * data = get_stream_from_map(map, name);
    free(name);

    if (data != NULL && max_subscribers_exceeded(map, data)) {
        int mutex_lock_err = pthread_mutex_unlock(&data->access_lock);
        assert(mutex_lock_err == 0);
        return NULL;
    } else {
        return data;
    }
}

void add_srt_subscriber(
        struct published_stream_map * map,
        struct authenticator * auth, char * name, char * addr, SRTSOCKET sock)
{
    struct published_stream_data * data = 
        add_subscriber_common(map, auth, name, addr);

    if (data == NULL) {
        srt_close(sock);
    } else {
        // Set socket to nonblocking mode
        bool no = false;
        int set_flag_err = srt_setsockflag(sock, SRTO_SNDSYN, &no, sizeof(no));

        if (set_flag_err != SRT_ERROR) {
            add_srt_subscriber_to_stream(data, sock);
        } else {
            srt_close(sock);
        }
        int mutex_lock_err = pthread_mutex_unlock(&data->access_lock);
        assert(mutex_lock_err == 0);
    }
}

const char * WEB_SUBSCRIBER_RESPONSE = 
    "HTTP/1.1 200 OK\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Content-Type: video/mp2t\r\n"
    "Transfer-Encoding: chunked\r\n\r\n";

void add_web_subscriber(
        struct published_stream_map * map,
        struct authenticator * auth, char * name, char * addr, int sock)
{
    struct published_stream_data * data = 
        add_subscriber_common(map, auth, name, addr);

    if (data == NULL) {
        close(sock);
    } else {
        // Send data as soon as possible to reduce latency
        int yes = 1;
        int set_opt_err =
            setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &yes, sizeof(yes));

        write(sock, WEB_SUBSCRIBER_RESPONSE, strlen(WEB_SUBSCRIBER_RESPONSE));

        // Set socket to nonblocking mode
        int set_access_mode_err = fcntl(sock, F_SETFL, O_NONBLOCK);

        if (set_access_mode_err == 0 && set_opt_err >= 0) {
            add_web_subscriber_to_stream(data, sock);
        } else {
            close(sock);
        }
        int mutex_lock_err = pthread_mutex_unlock(&data->access_lock);
        assert(mutex_lock_err == 0);
    }
}

void remove_stream_from_map(
        struct published_stream_map * map, struct published_stream_data * data)
{
    srt_close(data->sock);

    remove_name_from_map(map, data->name);

    int mutex_lock_err;
    mutex_lock_err = pthread_mutex_unlock(&data->access_lock);
    assert(mutex_lock_err == 0);

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
    free(data->name);
    free(data);
}
