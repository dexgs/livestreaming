#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include "authenticator.h"

struct authenticator {
    pthread_mutex_t num_connections_lock;
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

    auth->num_connections_lock = num_connections_lock;
    auth->max_pending_connections = max_pending_connections;
    auth->num_pending_connections = 0;
    auth->auth_command = auth_command;

    return auth;
}

bool contains_illegal_chars(const char * str, size_t str_len) {
    // Sanitize command as `popen` implicitly calls `sh -c`
    for (size_t i = 0; i < str_len; i++) {
        char c = str[i];
        if (
                c == '$' || c == '(' || c == ')' || c == '[' || c == ']'
                || c == '<' || c == '>' || c == '|' || c == '\n' || c == '\\'
                || c == '&' || c == '*' || c == '#' || c == '~' || c == '!'
                || c == '`' || c == ';')
        {
            return true;
        }
    }
    return false;
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

    char * type_str;
    if (is_publisher) {
        type_str = PUBLISH_STRING;
    } else {
        type_str = SUBSCRIBE_STRING;
    }

    size_t name_len = strlen(stream_name);
    size_t addr_len = strlen(addr);

    size_t command_len =
        strlen(auth->auth_command)
        + strlen(type_str)
        + addr_len
        + name_len
        + 6;
    char * command = malloc(sizeof(char) * command_len);
    // Set first char to the null character to avoid garbage data causing
    // problems
    command[0] = '\0';
    // This is sub-optimal, but I don't care right now.
    strcat(command, auth->auth_command);
    strcat(command, " ");
    strcat(command, type_str);
    strcat(command, " ");
    strcat(command, addr);
    strcat(command, " '");
    strcat(command, stream_name);
    strcat(command, "'");

    // Sanitize command as `popen` implicitly calls `sh -c`
    if (
            contains_illegal_chars(addr, addr_len)
            || contains_illegal_chars(stream_name, name_len))
    {
        free(command);
        return NULL;
    }


    // call `auth_command` with arguments `type_str addr stream_name`
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

        int exit_status = pclose(p);

        if (exit_status != 0) {
            // If command did not exit successfully
            free(output_stream_name);
            output_stream_name = NULL;
        } else if (chars_so_far == 0) {
            // If command had no output, default to stream_name
            output_stream_name = realloc(output_stream_name, inc + 1);
            output_stream_name = strcpy(output_stream_name, stream_name);
        }
    }

    free(command);

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



char * sockaddr_to_string(struct sockaddr * addr, int addr_len) {
    char * addr_str = malloc(sizeof(char) * (NI_MAXHOST + 1 + NI_MAXSERV));
    char * port_str = malloc(sizeof(char) * NI_MAXSERV);
    getnameinfo(
            (struct sockaddr *)addr, addr_len, addr_str,
            NI_MAXHOST, port_str, NI_MAXSERV, NI_NUMERICHOST);
    strcat(addr_str, ":");
    strcat(addr_str, port_str);
    free(port_str);
    addr_str = realloc(addr_str, strlen(addr_str) + 1);
    return addr_str;
}
