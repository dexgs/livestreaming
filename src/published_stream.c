#include <stdio.h>

#include "authenticator.h"
#include "guard.h"
#include "published_stream.h"
#include "srt.h"


void increment_num_subscribers(struct published_stream_data * data, int inc) {
    GUARD(&data->num_subscribers_lock, {
        data->num_subscribers += inc;
    })
}


static struct published_stream_data * create_published_stream_data(
        SRTSOCKET sock, char * name)
{
    struct published_stream_data * data =
        malloc(sizeof(struct published_stream_data));

    *data = (struct published_stream_data) { .sock = sock, .name = name };

    int mutex_init_err = 0;

    mutex_init_err |= pthread_mutex_init(&data->num_subscribers_lock, NULL);
    mutex_init_err |= pthread_mutex_init(&data->srt_subscribers_lock, NULL);
    mutex_init_err |= pthread_mutex_init(&data->web_subscribers_lock, NULL);
    mutex_init_err |= pthread_mutex_init(&data->access_lock, NULL);

    assert(mutex_init_err == 0);
    return data;
}


unsigned int get_num_subscribers(struct published_stream_data * data) {
    unsigned int num_subscribers;

    GUARD(&data->num_subscribers_lock, {
        num_subscribers = data->num_subscribers;
    });

    return num_subscribers;
}


void add_srt_subscriber_to_stream(
        struct published_stream_data * data, SRTSOCKET sock) 
{
    struct srt_subscriber_node * subscriber =
        malloc(sizeof(struct srt_subscriber_node));

    *subscriber = (struct srt_subscriber_node) { .sock = sock };

    GUARD(&data->srt_subscribers_lock, {
        subscriber->next = data->srt_subscribers;
        if (data->srt_subscribers != NULL) {
            data->srt_subscribers->prev = subscriber;
        }
        data->srt_subscribers = subscriber;
    })

    increment_num_subscribers(data, 1);

#ifndef NDEBUG
    printf("Added SRT subscriber %i to stream %s\n", sock, data->name);
#endif
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

#ifndef NDEBUG
    printf("Removed SRT subscriber %i from stream %s\n", subscriber->sock, data->name);
#endif

    // Free memory
    free(subscriber);
}


void add_web_subscriber_to_stream(
        struct published_stream_data * data, int sock)
{
    struct web_subscriber_node * subscriber =
        malloc(sizeof(struct web_subscriber_node));

    *subscriber = (struct web_subscriber_node) { .sock = sock };

    GUARD(&data->web_subscribers_lock, {
        subscriber->next = data->web_subscribers;
        if (data->web_subscribers != NULL) {
            data->web_subscribers->prev = subscriber;
        }
        data->web_subscribers = subscriber;
    })

    increment_num_subscribers(data, 1);

#ifndef NDEBUG
    printf("Added web subscriber %i to stream %s\n", sock, data->name);
#endif
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

#ifndef NDEBUG
    printf("Removed web subscriber %i from stream %s\n", subscriber->sock, data->name);
#endif

    // Free memory
    free(subscriber);
}


// djb2
static unsigned long hash(const char * str) {
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

    *map = (struct published_stream_map) {
        .max_publishers = max_publishers,
        .max_subscribers_per_publisher = max_subscribers_per_publisher
    };

    int mutex_init_err = pthread_mutex_init(&map->map_lock, NULL);
    assert(mutex_init_err == 0);

    return map;
}


// This function assumes map->map_lock is locked by the current thread
static struct published_stream_node * get_node_with_name(
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
    bool max_publishers_exceeded;

    GUARD(&map->map_lock, {
        max_publishers_exceeded =
            map->max_publishers > 0 && map->num_publishers >= map->max_publishers;
    })

    return max_publishers_exceeded;
}


bool stream_name_in_map(struct published_stream_map * map, const char * name) {
    bool stream_name_in_map;

    GUARD(&map->map_lock, {
        stream_name_in_map = get_node_with_name(map, name) != NULL;
    })

    return stream_name_in_map;
}


unsigned int num_published_streams(struct published_stream_map * map) {
    unsigned int num_streams;

    GUARD(&map->map_lock, {
        num_streams = map->num_publishers;
    })

    return num_streams;
}


// Simple quicksort impl for streams list
struct stream_info {
    char * name;
    unsigned int num_subscribers;
};

// Using Hoare's partition scheme
static unsigned int partition_stream_info(
        struct stream_info * streams, unsigned int len, unsigned int pivot)
{
    struct stream_info pivot_info = streams[pivot];
    int l = -1;
    int h = len;

    while (true) {
        do {
            l++;
        } while (streams[l].num_subscribers > pivot_info.num_subscribers);

        do {
            h--;
        } while (streams[h].num_subscribers < pivot_info.num_subscribers);

        if (l >= h) {
            return l;
        }
        // swap
        struct stream_info temp = streams[l];
        streams[l] = streams[h];
        streams[h] = temp;
    }
}

static void sort_stream_info(
        struct stream_info * streams, unsigned int len)
{
    if (len >= 2) {
        unsigned int pivot = len / 2;
        pivot = partition_stream_info(streams, len, pivot);
        sort_stream_info(streams, pivot);
        sort_stream_info(streams + pivot, len - pivot);
    }
}

// Return list of stream names sorted by number of watchers. The returned
// pointer is a single allocation.
char ** stream_names(
        struct published_stream_map * map, unsigned int * num_streams)
{
    unsigned int streams_len;
    GUARD(&map->map_lock, {
        streams_len = map->num_publishers;
    })

    if (streams_len == 0) {
        *num_streams = 0;
        return NULL;
    }

    struct stream_info * streams =
        malloc(sizeof(struct stream_info) * streams_len);

    unsigned int streams_index = 0;

    // Collect list of stream info
    for (int i = 0; i < MAP_SIZE; i++) {
        GUARD(&map->map_lock, {
            struct published_stream_node * node = map->buckets[i];

            while (node != NULL) {
                // If stream is not unlisted
                if (!(
                        strlen(node->data->name) >= strlen(UNLISTED_STREAM_NAME_PREFIX)
                        && strncmp(
                            UNLISTED_STREAM_NAME_PREFIX, node->data->name,
                            strlen(UNLISTED_STREAM_NAME_PREFIX)) == 0))
                {
                    struct stream_info info;
                    info.name = strdup(node->data->name);
                    GUARD(&node->data->num_subscribers_lock, {
                        info.num_subscribers = node->data->num_subscribers;
                    })

                    // If there isn't enough room to add another stream
                    if (streams_index >= streams_len) {
                        streams_len *= 2;
                        streams =
                            realloc(streams, sizeof(struct stream_info) * streams_len);
                    }

                    streams[streams_index] = info;
                    streams_index++;
                }
                node = node->next;
            }
        })
    }

    if (streams_index == 0) {
        free(streams);
        return NULL;
    }

    // Sort stream info by number of subscribers (quicksort)
    sort_stream_info(streams, streams_index);

    // Pack the sorted stream list into a single allocation
    size_t base_size = sizeof(char *) * streams_index;
    size_t capacity = 32 * streams_index;
    size_t size = 0;
    char ** stream_names = malloc(base_size + capacity);
    char * stream_names_buf = (char *) (stream_names + streams_index);

    for(size_t i = 0; i < streams_index; i++) {
        size_t name_len = strlen(streams[i].name) + 1;

        if (capacity < size + name_len) {
            while (capacity < size + name_len) {
                capacity *= 2;
            }
            stream_names = realloc(stream_names, base_size + capacity);
            stream_names_buf = (char *) (stream_names + streams_index);
        }

        strcpy(stream_names_buf + size, streams[i].name);
        stream_names[i] = stream_names_buf + size;

        size += name_len;
        free(streams[i].name);
    }
    free(streams);

    *num_streams = streams_index;
    return stream_names;
}


struct published_stream_data * create_stream_data_in_map(
        struct published_stream_map * map, SRTSOCKET sock, char * name)
{
    struct published_stream_data * data;

    GUARD(&map->map_lock, {
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
    })

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
    GUARD(&map->map_lock, {
        int index = hash(name) % MAP_SIZE;

        struct published_stream_node * node = map->buckets[index];
        while (node != NULL) {
            if (strcmp(name, node->data->name) == 0) {
                break;
            }
            node = node->next;
        }

        if (node != NULL) {
            GUARD(&node->data->access_lock, {
                if (node == map->buckets[index]) map->buckets[index] = node->next;
                if (node->prev != NULL) node->prev->next = node->next;
                if (node->next != NULL) node->next->prev = node->prev;
                free(node);
            })
        }

        map->num_publishers--;
    })
}


struct published_stream_data * get_stream_from_map(
        struct published_stream_map * map, const char * name)
{
    struct published_stream_data * data = NULL;

    GUARD(&map->map_lock, {
        struct published_stream_node * node = get_node_with_name(map, name);
        if (node != NULL) {
            data = node->data;
            /* The data's access lock is locked while map lock is also locked
             * to prevent a use-after-free if the data were to be removed
             * from the map after this function returns but before the caller
             * uses the data.
             *
             * Doing it this way guarantees that the data will not be freed
             * before the caller of this function indicates they are done with
             * the data by unlocking its access lock. */
            int mutex_lock_err = pthread_mutex_lock(&data->access_lock);
            if (mutex_lock_err != 0) data = NULL;
        }
    })

    return data;
}

bool max_subscribers_exceeded(
        struct published_stream_map * map, struct published_stream_data * data)
{
    bool max_subscribers_exceeded;

    GUARD(&data->num_subscribers_lock, {
        max_subscribers_exceeded =
            map->max_subscribers_per_publisher > 0
            && data->num_subscribers >= map->max_subscribers_per_publisher;
    })

    return max_subscribers_exceeded;
}

// Common setup between adding an SRT subscriber and adding a web subscriber
static struct published_stream_data * add_subscriber_common(
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

const char * WEB_SUBSCRIBER_OK = 
    "HTTP/1.1 200 OK\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Content-Type: video/mp2t\r\n"
    "Transfer-Encoding: chunked\r\n\r\n";

const char * WEB_SUBSCRIBER_NOT_FOUND =
    "HTTP/1.1 404 Not Found\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 48\r\n"
    "Connection: close\r\n\r\n"
    "There is currently no active stream at this URL.";

void add_web_subscriber(
        struct published_stream_map * map,
        struct authenticator * auth, char * name, char * addr, int sock)
{
    struct published_stream_data * data = 
        add_subscriber_common(map, auth, name, addr);

    if (data == NULL) {
        write(sock, WEB_SUBSCRIBER_NOT_FOUND, strlen(WEB_SUBSCRIBER_NOT_FOUND));
        close(sock);
    } else {
        write(sock, WEB_SUBSCRIBER_OK, strlen(WEB_SUBSCRIBER_OK));
        add_web_subscriber_to_stream(data, sock);

        int mutex_lock_err = pthread_mutex_unlock(&data->access_lock);
        assert(mutex_lock_err == 0);
    }
}

void remove_stream_from_map(
        struct published_stream_map * map, struct published_stream_data * data)
{
    srt_close(data->sock);

    remove_name_from_map(map, data->name);

    GUARD(&data->access_lock, {
        // Clean up SRT subscribers
        GUARD(&data->srt_subscribers_lock, {
            struct srt_subscriber_node * srt_node = data->srt_subscribers;
            while (srt_node != NULL) {
                srt_close(srt_node->sock);
                struct srt_subscriber_node * next_node = srt_node->next;
                free(srt_node);
                srt_node = next_node;
            }
        })

        // Clean up Web subscribers
        GUARD(&data->web_subscribers_lock, {
            struct web_subscriber_node * web_node = data->web_subscribers;
            while (web_node != NULL) {
                write(web_node->sock, "0\r\n\r\n", 5);
                close(web_node->sock);
                struct web_subscriber_node * next_node = web_node->next;
                free(web_node);
                web_node = next_node;
            }
        })

        // Free the published_stream_data
        pthread_mutex_destroy(&data->num_subscribers_lock);
        pthread_mutex_destroy(&data->srt_subscribers_lock);
        pthread_mutex_destroy(&data->web_subscribers_lock);
        free(data->name);
    })

    pthread_mutex_destroy(&data->access_lock);
    free(data);
}
