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
    for (int i = 0; i < NUM_TEST_RUNS; i ++) {
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


// MAIN

int main() {
    printf("Running tests...\n");

    printf("Testing authenticator\n");
    test_authenticator();
}
