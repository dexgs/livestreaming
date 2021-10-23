#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include "../src/published_stream.h"
#include "../src/authenticator.h"

// When relevant, how many threads should be spawned by any given test
#define NUM_TEST_THREADS 20
// How many times each test will run
#define NUM_TEST_RUNS 10


/* These tests are intended to make sure the code I have written is behaving
 * properly. The tests do not cover the behaviour of third-party code like SRT
 * or PicoHTTPParser, as their correctness is not my responsibility */


// AUTHENTICATOR TESTS

struct test_auth_thread_data {
    struct authenticator * auth;
    const char * input;
    const char * expected_name;
    bool may_reject;
    bool must_reject;
};

// Test that `authenticate` returns expected output. Only one input is given for
// both the `name` and `addr` parameters of authenticate because whether or not
// their values are distinct is irrelevant to the correctness of authenticator's
// behaviour.
void * test_authenticator_thread(void * _d) {
    struct test_auth_thread_data * d = (struct test_auth_thread_data *) _d;
    struct authenticator * auth = d->auth;
    const char * input = d->input;
    const char * expected_name = d->expected_name;
    bool may_reject = d->may_reject;
    bool must_reject = d->must_reject;

    // Test with input data as name and addr
    char * auth_output;

    bool modes[2] = {true, false};
    for (int i = 0; i < 2; i++) {
        auth_output = authenticate(auth, modes[i], input, input);

        if (must_reject) {
            // If rejection is the only correct output
            assert(auth_output == NULL);
        } else if (may_reject) {
            // If rejection OR `expected_name` is a correct output
            assert(auth_output == NULL || strcmp(auth_output, expected_name) == 0);
        } else {
            // If `expected_name` is the only correct output
            assert(strcmp(auth_output, expected_name) == 0);
        }

        if (auth_output != NULL) free(auth_output);
    }

    return NULL;
}

// Start threads which call `test_authenticator_thread`. This makes sure
// authenticator's behaviour is both correct and thread-safe.
void test_authenticator_thread_driver(
        struct authenticator * auth, char * input,
        const char * expected_name, bool expect_input_name,
        bool may_reject, bool must_reject)
{
    // If no input is given, use a garbage string
    if (input == NULL) {
        // Make a string from garbage data that
        // is guaranteed to be null terminated
        int garbage_len = 20;
        char * garbage = malloc(garbage_len);
        garbage[garbage_len - 1] = '\0';
        input = garbage;
    } else {
        input = strdup(input);
    }

    struct test_auth_thread_data * d = malloc(sizeof(struct test_auth_thread_data));
    d->auth = auth;
    d->input = input;
    // If the expected output equals the input
    if (expect_input_name) {
        d->expected_name = d->input;
    } else {
        d->expected_name = expected_name;
    }
    d->may_reject = may_reject;
    d->must_reject = must_reject;

    int pthread_err;
    pthread_t handles[NUM_TEST_THREADS];
    for (int i = 0; i < NUM_TEST_THREADS; i++) {
        pthread_err =
            pthread_create(&handles[i], NULL, test_authenticator_thread, d);
        assert(pthread_err == 0);
    } 
    for(int i = 0; i < NUM_TEST_THREADS; i++) {
        pthread_err = pthread_join(handles[i], NULL);
        assert(pthread_err == 0);
    }

    free(input);
    free(d);
}

void test_authenticator() {
    for (int i = 0; i < NUM_TEST_RUNS; i++) {
        // Test authenticator which always accepts and sets names to "test_name"
        struct authenticator * constant_name_auth =
            create_authenticator("echo -n test_name; exit 0", 0);
        test_authenticator_thread_driver(
                constant_name_auth, NULL, "test_name", false, true, false);
        // Test with empty string
        test_authenticator_thread_driver(
                constant_name_auth, "", "test_name", false, false, false);
        free(constant_name_auth);

        // Test authenticator which always rejects
        struct authenticator * always_reject_auth =
            create_authenticator("exit 1", 0);
        test_authenticator_thread_driver(
                always_reject_auth, NULL, NULL, false, false, true);
        free(always_reject_auth);

        // Test an authenticator which always accepts and does not change names
        struct authenticator * always_accept_auth =
            create_authenticator("exit 0", 0);
        test_authenticator_thread_driver(
                always_accept_auth, NULL, NULL, true, true, false);
        test_authenticator_thread_driver(
                always_accept_auth, "foo", "foo", true, false, false);
        // If input contains special shell characters, authenticate will reject
        // no matter what to prevent malicious input hijacking its internal call
        // to `popen` to do something like run a fork bomb >:)
        test_authenticator_thread_driver(
                always_accept_auth, ":(){ :|:& };:", NULL, false, false, true);
        test_authenticator_thread_driver(
                always_accept_auth, "$(echo pwned!!)", NULL, false, false, true);
        free(always_accept_auth);
    }
}


// PUBLISHED STREAM TESTS

struct published_stream_thread_data {
    struct published_stream_map * map;
    const char * name;
};

void * add_stream_to_map_thread(void * _d) {
    struct published_stream_thread_data * d =
        (struct published_stream_thread_data *) _d;
    struct published_stream_map * map = d->map;
    const char * name = d->name;
    free(d);

    // We're not actually doing any I/O, so just use a dummy file descriptior
    add_stream_to_map(map, -1, name);

    return NULL;
}

void * add_srt_subscriber_to_stream_thread(void * _data) {
    struct published_stream_data * data = (struct published_stream_data *) _data;
    add_srt_subscriber(data, -1);

    return NULL;
}

void * add_web_subscriber_to_stream_thread(void * _data) {
    struct published_stream_data * data = (struct published_stream_data *) _data;
    add_web_subscriber(data, -1);

    return NULL;
}

void test_published_streams() {
    for (int i = 0; i < NUM_TEST_RUNS; i++) {
        // Make stream names from garbage data
        char ** stream_names = malloc(sizeof(void *) * NUM_TEST_THREADS);
        for (int i = 0; i < NUM_TEST_THREADS; i++) {
            char * stream_name = malloc(20);
            stream_name[19] = '\0';
            sprintf(stream_name, "%d", i);
            stream_names[i] = stream_name;
        }

        struct published_stream_map * map =
            create_published_stream_map(NUM_TEST_THREADS, NUM_TEST_THREADS);

        int pthread_err;

        // Add NUM_TEST_THREADS streams to the map concurrently
        pthread_t handles[NUM_TEST_THREADS];
        for (int i = 0; i < NUM_TEST_THREADS; i++) {
            struct published_stream_thread_data * d =
                malloc(sizeof(struct published_stream_thread_data));
            d->map = map;
            d->name = stream_names[i];
            pthread_err =
                pthread_create(&handles[i], NULL, add_stream_to_map_thread, d);
            assert(pthread_err == 0);
        }
        for (int i = 0; i < NUM_TEST_THREADS; i++) {
            pthread_err = pthread_join(handles[i], NULL);
        }

        // Assert everything was properly added to the map.
        // Recall that the capacity of the map was set to NUM_TEST_THREADS
        assert(max_publishers_exceeded(map));

        // Assert that the stream map does not allow duplicate stream names
        for(int i = 0; i < NUM_TEST_THREADS; i++) {
            struct published_stream_data * data =
                add_stream_to_map(map, -1, stream_names[i]);
            assert(data == NULL);
        }

        // Get a list of all the stream data
        struct published_stream_data ** streams =
            malloc(sizeof(void *) * NUM_TEST_THREADS);
        for (int i = 0; i < NUM_TEST_THREADS; i++) {
            struct published_stream_data * data =
                get_stream_from_map(map, stream_names[i]);
            assert(data != NULL);
            pthread_mutex_unlock(&data->access_lock);
            streams[i] = data;
        }

        for (int i = 0; i < NUM_TEST_THREADS; i++) {
            for (int j = 0; j < NUM_TEST_THREADS; j++) {
                if (j % 2 == 0) {
                    pthread_err = pthread_create(
                            &handles[j], NULL,
                            add_srt_subscriber_to_stream_thread, streams[i]);
                } else {
                    pthread_err = pthread_create(
                            &handles[j], NULL,
                            add_web_subscriber_to_stream_thread, streams[i]);
                }
                assert(pthread_err == 0);
            }
            for(int j = 0; j < NUM_TEST_THREADS; j++) {
                pthread_err = pthread_join(handles[j], NULL);
                assert(pthread_err == 0);
            }
            assert(max_subscribers_exceeded(map, streams[i]));

            struct srt_subscriber_node * srt_node = streams[i]->srt_subscribers;
            while (srt_node != NULL) {
                struct srt_subscriber_node * next_node = srt_node->next;
                free(srt_node);
                srt_node = next_node;
                increment_num_subscribers(streams[i], -1);
            }

            struct web_subscriber_node * web_node = streams[i]->web_subscribers;
            while (web_node != NULL) {
                struct web_subscriber_node * next_node = web_node->next;
                free(web_node);
                web_node = next_node;
                increment_num_subscribers(streams[i], -1);
            }

            assert(get_num_subscribers(streams[i]) == 0);
        }

        // CLEAN UP

        // Clean up published_stream_data
        for (int i = 0; i < NUM_TEST_THREADS; i++) {
            struct published_stream_data * data = streams[i];
            remove_stream_from_map(map, stream_names[i]);
            pthread_mutex_unlock(&data->access_lock);
            // Clean up mutexes and free
            pthread_mutex_destroy(&data->num_subscribers_lock);
            pthread_mutex_destroy(&data->srt_subscribers_lock);
            pthread_mutex_destroy(&data->web_subscribers_lock);
            pthread_mutex_destroy(&data->access_lock);
            free(data);
        }

        // Assert everything was properly removed from the map
        assert(!max_publishers_exceeded(map));

        // Clean up names
        for (int i = 0; i < NUM_TEST_THREADS; i++) {
            free(stream_names[i]);
        }
        free(stream_names);

        free(streams);
        free(map);
    }
}


// MAIN

int main() {
    printf("Running tests...\n");

    printf("Testing authenticator\n");
    test_authenticator();

    printf("Testing published streams\n");
    test_published_streams();
}
