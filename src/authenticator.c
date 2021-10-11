#include <pthread.h>
#include <stdbool.h>
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

bool authenticate(
        struct authenticator * auth, enum connection_type type,
        const char * stream_name, const char * password, const char * addr)
{
    bool authentication_succeeded = false;

    int mutex_lock_err;
    mutex_lock_err = pthread_mutex_lock(&auth->num_connections_lock);
    assert(mutex_lock_err == 0);
    auth->num_pending_connections++;
    mutex_lock_err = pthread_mutex_unlock(&auth->num_connections_lock);
    assert(mutex_lock_err == 0);

    mutex_lock_err = pthread_mutex_lock(&auth->process_lock);
    assert(mutex_lock_err == 0);

    pid_t pid = fork();
    if (pid == 0) {
        char * type_str;
        switch (type) {
            case publish:
                type_str = PUBLISH_STRING;
                break;
            case subscribe:
                type_str = SUBSCRIBE_STRING;
                break;
        }
        execl(
                auth->auth_command, auth->auth_command,
                type_str, stream_name, addr, password, NULL);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        if (exit_status == 0) authentication_succeeded = true;
    }

    mutex_lock_err = pthread_mutex_unlock(&auth->process_lock);
    assert(mutex_lock_err == 0);

    mutex_lock_err = pthread_mutex_lock(&auth->num_connections_lock);
    assert(mutex_lock_err == 0);
    auth->num_pending_connections--;
    mutex_lock_err = pthread_mutex_unlock(&auth->num_connections_lock);
    assert(mutex_lock_err == 0);

    return authentication_succeeded;
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
