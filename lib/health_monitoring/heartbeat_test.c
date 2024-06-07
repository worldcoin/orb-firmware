#include <heartbeat.h>
#include <zephyr/ztest.h>

static bool timed_out = false;

int
timeout_cb(void)
{
    // declare unused stacked variable to ensure the
    // callback can do _enough_ work without stack overflow
    // we assume the callback to not use more than half of the stack
    volatile char unused[THREAD_STACK_SIZE_HEARTBEAT / 2] = {0};
    UNUSED(unused);

    timed_out = true;

    return 0;
}

ZTEST(hil, test_heartbeat)
{
    // register timeout callback
    // which will allow us to check if the heartbeat has timed out
    heartbeat_register_cb(timeout_cb);

    // timed_out = false;
    zassert_false(timed_out);

    // start with 5s delay
    heartbeat_boom(5);
    k_sleep(K_SECONDS(2));
    zassert_false(timed_out);

    // push with already started
    heartbeat_boom(5);
    zassert_false(timed_out);

    // stop, with 0 delay
    heartbeat_boom(0);
    zassert_false(timed_out);

    // stop with already stopped
    heartbeat_boom(0);
    zassert_false(timed_out);

    k_sleep(K_SECONDS(2));

    // restart with 2s delay, and wait for timeout
    heartbeat_boom(2);
    k_sleep(K_SECONDS(3));
    zassert_true(timed_out);

    heartbeat_boom(0);
}
