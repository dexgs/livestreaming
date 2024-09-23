#include <assert.h>
#include <ctype.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "authenticator.h"
#include "guard.h"


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

// This function assumes num_connections_lock is currently held
static bool max_pending_connections_exceeded(struct authenticator * auth) {
    if (auth->max_pending_connections == 0) {
        return false;
    } else {
        return auth->num_pending_connections >= auth->max_pending_connections;
    }
}

static bool is_url_safe(const char * str) {
    const char allowed_chars[] = { '%', '-', '_', '.', '~' };
    size_t len = sizeof(allowed_chars) / sizeof(allowed_chars[0]);

    char c;
    while ((c = *str++)) {
        // every character must be either alphanumeric or one of the chars
        // listed above

        if (!isalnum(c)) {
            bool is_allowed = false;
            for (size_t i = 0; i < len; i++) {
                if (c == allowed_chars[i]) {
                    is_allowed = true;
                    break;
                }
            }
            if (!is_allowed) return false;
        }
    }

    return true;
}

static bool contains_illegal_chars(const char * str) {
    // Sanitize command as `popen` implicitly calls `sh -c`
    for (size_t i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        switch (c) {
                case '$': case '(': case ')': case '[': case ']':
                case '<': case '>': case '|': case '\n': case '\\':
                case '&': case '*': case '#': case '~': case '!':
                case '`': case ';': case '\'': case '"':
                    return true;
        }
    }
    return false;
}

const char * PUBLISH_STRING = "PUBLISH";
const char * SUBSCRIBE_STRING = "SUBSCRIBE";

char * authenticate(
        struct authenticator * auth, bool is_publisher,
        const char * addr, const char * stream_name)
{
    char * output_stream_name = NULL;

    bool exceeded;
    GUARD(&auth->num_connections_lock, {
        exceeded = max_pending_connections_exceeded(auth);
        if (!exceeded) {
            auth->num_pending_connections++;
        }
    });
    if (exceeded) return NULL;

    const char * type_str = is_publisher ? PUBLISH_STRING : SUBSCRIBE_STRING;

    size_t command_len =
        strlen(auth->auth_command)
        + strlen(type_str)
        + strlen(addr)
        + strlen(stream_name)
        + 6;
    char * command = malloc(command_len);

    snprintf(command, command_len, "%s %s %s '%s'",
            auth->auth_command, type_str, addr, stream_name);

    // Sanitize command as `popen` implicitly calls `sh -c`
    if (
            contains_illegal_chars(addr)
            || contains_illegal_chars(stream_name)
            || !is_url_safe(stream_name))
    {
        free(command);
        return NULL;
    }


    // call `auth_command` with arguments `type_str addr stream_name`
    FILE * p;
    p = popen(command, "r");
    free(command);

    // If the command was started successfullly
    if (p != NULL) {
        size_t inc = strlen(stream_name) + 1;
        if (inc < 10) inc = 10;
        size_t output_stream_name_len = inc;
        output_stream_name = malloc(output_stream_name_len);

        size_t chars_so_far = 0;
        signed char ch;
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
            free(output_stream_name);
            output_stream_name = strdup(stream_name);
        }
    }

    GUARD(&auth->num_connections_lock, {
        auth->num_pending_connections--;
    })

    return output_stream_name;
}


char * sockaddr_to_string(struct sockaddr * addr, int addr_len) {
    char * addr_str = malloc(NI_MAXHOST + 1 + NI_MAXSERV);
    char * port_str = malloc(NI_MAXSERV);
    getnameinfo(
            (struct sockaddr *)addr, addr_len, addr_str,
            NI_MAXHOST, port_str, NI_MAXSERV, NI_NUMERICHOST);
    strcat(strcat(addr_str, ":"), port_str);
    free(port_str);
    addr_str = realloc(addr_str, strlen(addr_str) + 1);
    return addr_str;
}
