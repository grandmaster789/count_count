#include <catch2/catch_test_macros.hpp>

#include "async/just.h"
#include "async/then.h"
#include "async/sync_wait.h"
#include "async/run_loop_context.h"
#include "async/thread_context.h"
#include "async/cout_receiver.h"

TEST_CASE("Just", "[async]") {
    using namespace cc::async;

    auto work = just(123);
    CoutReceiver receiver;
    auto operational_state = connect(work, receiver);

    start(operational_state);
}

TEST_CASE("Then", "[async]") {
    using namespace cc::async;

    auto work = then(
        just(100),
        [](int x) {
            return x + 1;
        }
    );

    CoutReceiver receiver;
    auto operational_state = connect(work, receiver);

    start(operational_state);
}

TEST_CASE("SyncWait", "[async]") {
    using namespace cc::async;

    auto work = then(
        just(200),
        [](int x) {
            return x + 1;
        }
    );
    int result = sync_wait(work).value();

    REQUIRE(result == 201);
}

TEST_CASE("RunLoopContext", "[async]") {
    using namespace cc::async;

    RunLoop loop;

    auto loop_scheduler = loop.get_scheduler();

    auto work1 = then(schedule(loop_scheduler), [] (auto) { return 1; } );
    auto operational_state1 = connect(work1, CoutReceiver{});
    start(operational_state1); // this will enqueue the work in the schedule

    auto work2 = then(schedule(loop_scheduler), [] (auto) { return 2; } );
    auto operational_state2 = connect(work2, CoutReceiver{});
    start(operational_state2);

    loop.finish(); // indicate that we're done setting up the workload
    loop.run();    // run the enqueued work
}

TEST_CASE("ThreadContext", "[async]") {
    using namespace cc::async;

    ThreadContext ctx;

    auto scheduler = ctx.get_scheduler();

    auto work1 = then(schedule(scheduler), [] (auto) { return 3; } ); // put some work onto the thread context
    auto work2 = then(work1, [](int i) { return i + 1; });            // when the above task completes, perform extra work on it
    auto final_result = sync_wait(work2);                             // collect the final result

    ctx.finish(); // we're done setting up the workload
    ctx.join();   // wait for the thread to complete

    REQUIRE(final_result.value() == 4);
}