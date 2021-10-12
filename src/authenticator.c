#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "authenticator.h"

struct authenticator {
    pthread_mutex_t num_connections_lock;
    pthread_mutex_t process_lock;
    unsigned int max_pending_connections;
    unsigned int num_pending_connections;
    const char * auth_command;
};

struct authenticator * create_authenticator(
        const char * auth_command, unsigned int max_pending_connections)
{
    struct authenticator * auth = malloc(sizeof(struct authenticator));

    int mutex_init_err;
    pthread_mutex_t num_connections_lock;
    mutex_init_err = pthread_mutex_init(&num_connections_lock, NULL);
    assert(mutex_init_err == 0);

    pthread_mutex_t process_lock;
    mutex_init_err = pthread_mutex_init(&process_lock, NULL);
    assert(mutex_init_err == 0);

    auth->num_connections_lock = num_connections_lock;
    auth->process_lock = process_lock;
    auth->max_pending_connections = max_pending_connections;
    auth->num_pending_connections = 0;
    auth->auth_command = auth_command;

    return auth;
}

#define PUBLISH_STRING "PUBLISH"
#define SUBSCRIBE_STRING "SUBSCRIBE"

char * authenticate(
        struct authenticator * auth, bool is_publisher,
        const char * addr, const char * stream_name)
{
    char * output_stream_name = NULL;

    int mutex_lock_err;
    mutex_lock_err = pthread_mutex_lock(&auth->num_connections_lock);
    assert(mutex_lock_err == 0);
    auth->num_pending_connections++;
    mutex_lock_err = pthread_mutex_unlock(&auth->num_connections_lock);
    assert(mutex_lock_err == 0);

    mutex_lock_err = pthread_mutex_lock(&auth->process_lock);
    assert(mutex_lock_err == 0);

    char * type_str;
    if (is_publisher) {
        type_str = PUBLISH_STRING;
    } else {
        type_str = SUBSCRIBE_STRING;
    }

    size_t command_len =
        strlen(auth->auth_command)
        + strlen(type_str)
        + strlen(addr)
        + strlen(stream_name)
        + 4;
    char * command = malloc(sizeof(char) * command_len);
    // This is sub-optimal, but I don't care right now.
    strcat(command, auth->auth_command);
    strcat(command, " ");
    strcat(command, type_str);
    strcat(command, " ");
    strcat(command, addr);
    strcat(command, " ");
    strcat(command, stream_name);

    int exit_status = 1;

    FILE * p;
    p = popen(command, "r");

    // If the command was started successfullly
    if (p != NULL) {
        size_t inc = strlen(stream_name);
        size_t output_stream_name_len = strlen(stream_name);
        output_stream_name = malloc(sizeof(char) * output_stream_name_len);

        size_t chars_so_far = 0;
        char ch;
        while ((ch = fgetc(p)) != EOF) {
            // If output_stream_name doesn't have room for ch
            if (output_stream_name_len <= chars_so_far) {
                output_stream_name_len += inc;
                output_stream_name =
                    realloc(output_stream_name, output_stream_name_len);
            }
            output_stream_name[chars_so_far] = ch;
            chars_so_far++;
        }
        // Make sure output_stream_name is terminated by the null character
        output_stream_name = realloc(output_stream_name, chars_so_far + 1);
        output_stream_name[chars_so_far] = '\0';

        exit_status = pclose(p);

        if (exit_status != 0) {
            // If command did not exit successfully
            free(output_stream_name);
            output_stream_name = NULL;
        } else if (chars_so_far == 0) {
            // If command had no output, default to stream_name
            output_stream_name = realloc(output_stream_name, inc);
            output_stream_name = strcpy(output_stream_name, stream_name);
        }
    }

    free(command);

    mutex_lock_err = pthread_mutex_unlock(&auth->process_lock);
    assert(mutex_lock_err == 0);

    mutex_lock_err = pthread_mutex_lock(&auth->num_connections_lock);
    assert(mutex_lock_err == 0);
    auth->num_pending_connections--;
    mutex_lock_err = pthread_mutex_unlock(&auth->num_connections_lock);
    assert(mutex_lock_err == 0);

    return output_stream_name;
}

bool max_pending_connections_exceeded(struct authenticator * auth) {
    if (auth->max_pending_connections == 0) return false;

    int mutex_lock_err;
    mutex_lock_err = pthread_mutex_lock(&auth->num_connections_lock);
    assert(mutex_lock_err == 0);

    bool exceeded;
    if (auth->num_pending_connections >= auth->max_pending_connections) {
        exceeded = true;
    } else {
        exceeded = false;
    }

    mutex_lock_err = pthread_mutex_unlock(&auth->num_connections_lock);
    assert(mutex_lock_err == 0);

    return exceeded;
}
