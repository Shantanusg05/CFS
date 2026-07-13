#include <catch2/catch_test_macros.hpp>

#include "../src/processService.hpp"
#include "../src/processLog.hpp"
#include "../src/queueService.hpp"
#include "../src/cpuBoundProcessExecution.hpp"
#include "../src/ioBoundProcessExecution.hpp"
#include "../src/cfs.hpp"
#include "../src/RoundRobin.hpp"

namespace {
Process makeProcess(int pid, int priority, int cpuBurstTime, PROCESS_NATURE nature, long long vruntime = 0) {
    Process p;
    p.pid = pid;
    p.priority = priority;
    p.cpu_burst_time = cpuBurstTime;
    p.processNature = nature;
    p.vruntime = vruntime;
    p.processState.counter = 0;
    return p;
}
}  // namespace

// --- weightFunction -------------------------------------------------------

TEST_CASE("weightFunction matches NICE_0_LOAD / (priority + 1)", "[math]") {
    REQUIRE(weightFunction(0) == 1024.0);
    REQUIRE(weightFunction(1) == 512.0);
    REQUIRE(weightFunction(3) == 256.0);
}

// --- executeCpuBoundProcess -------------------------------------------------

TEST_CASE("executeCpuBoundProcess decrements burst time and increases vruntime", "[cpu]") {
    Process p = makeProcess(1, 0, 5, PROCESS_NATURE::CPU_BOUND);
    QueueService q;

    executeCpuBoundProcess(&p, 1, q);

    REQUIRE(p.cpu_burst_time == 4);
    REQUIRE(p.vruntime > 0);
    REQUIRE_FALSE(q.is_empty());  // burst remains, so it should be requeued
}

TEST_CASE("Edge case: a process finishing its burst is not requeued", "[cpu][edge]") {
    Process p = makeProcess(1, 0, 1, PROCESS_NATURE::CPU_BOUND);
    QueueService q;

    executeCpuBoundProcess(&p, 1, q);

    REQUIRE(p.cpu_burst_time == 0);
    REQUIRE(q.is_empty());
}

TEST_CASE("Lower priority number (more favored, nice-value style) accumulates vruntime more slowly",
          "[fairness]") {
    // NOTE: in this codebase, weight = NICE_0_LOAD / (priority + 1), so a
    // SMALLER priority number means a LARGER weight and slower vruntime
    // growth -- this mirrors Linux's "nice value" convention (0 = default,
    // lower = more favored), not a scheme where a bigger number means
    // "more important."
    Process morefavored = makeProcess(1, /*priority=*/0, 5, PROCESS_NATURE::CPU_BOUND);
    Process lessfavored = makeProcess(2, /*priority=*/5, 5, PROCESS_NATURE::CPU_BOUND);
    QueueService q;

    executeCpuBoundProcess(&morefavored, 1, q);
    executeCpuBoundProcess(&lessfavored, 1, q);

    REQUIRE(morefavored.vruntime < lessfavored.vruntime);
}

// --- handleIoBoundProcess / sleeper fairness -------------------------------

TEST_CASE("handleIoBoundProcess clamps vruntime to the runqueue minimum (sleeper fairness)",
          "[io][fairness]") {
    // sleeper starts well above the runqueue's minimum vruntime.
    Process sleeper = makeProcess(1, 0, 5, PROCESS_NATURE::IO_BOUND, /*vruntime=*/200);
    Process runner = makeProcess(2, 0, 5, PROCESS_NATURE::CPU_BOUND, /*vruntime=*/50);
    QueueService q;
    q.push_element(&runner);

    handleIoBoundProcess(&sleeper, /*ioWaitTime=*/10, q);

    // Without the fix, vruntime would jump to roughly
    // 200 + 10*1024/1024 = 210, or worse for lower-priority tasks where the
    // I/O charge dominates. With the fix it should be clamped near the
    // runqueue minimum (50) plus the small post-wake CPU charge.
    REQUIRE(sleeper.vruntime <= 51);
    REQUIRE(sleeper.vruntime >= 50);
}

TEST_CASE("handleIoBoundProcess does not clamp below the runqueue minimum", "[io][edge]") {
    // sleeper already below the runqueue minimum — should not be raised.
    Process sleeper = makeProcess(1, 0, 5, PROCESS_NATURE::IO_BOUND, /*vruntime=*/0);
    Process runner = makeProcess(2, 0, 5, PROCESS_NATURE::CPU_BOUND, /*vruntime=*/50);
    QueueService q;
    q.push_element(&runner);

    handleIoBoundProcess(&sleeper, /*ioWaitTime=*/10, q);

    REQUIRE(sleeper.vruntime < 50);
}

// --- QueueService -----------------------------------------------------------

TEST_CASE("QueueService dispatches the lowest-vruntime process first", "[queue]") {
    Process high = makeProcess(1, 0, 1, PROCESS_NATURE::CPU_BOUND, 100);
    Process low = makeProcess(2, 0, 1, PROCESS_NATURE::CPU_BOUND, 10);
    QueueService q;

    q.push_element(&high);
    q.push_element(&low);

    REQUIRE(q.top_element()->pid == 2);
}

// --- cfs::schedule (integration) --------------------------------------------

TEST_CASE("cfs::schedule produces one log entry per executed time slice", "[cfs][integration]") {
    Process p = makeProcess(1, 0, 3, PROCESS_NATURE::CPU_BOUND);
    std::vector<Process*> processes = {&p};

    cfs scheduler;
    std::vector<ProcessLog*> logs = scheduler.schedule(processes);

    REQUIRE(logs.size() == 3);
    for (auto log : logs) {
        REQUIRE(log->pid == 1);
        delete log;  // cfs::schedule heap-allocates these; tests own cleanup
    }
}

TEST_CASE("Edge case: zero burst time produces no schedule entries", "[cfs][edge]") {
    Process p = makeProcess(1, 0, 0, PROCESS_NATURE::CPU_BOUND);
    std::vector<Process*> processes = {&p};

    cfs scheduler;
    std::vector<ProcessLog*> logs = scheduler.schedule(processes);

    REQUIRE(logs.empty());
}

TEST_CASE("Edge case: equal-priority processes interleave rather than one starving the other",
          "[cfs][edge][fairness]") {
    Process a = makeProcess(1, 0, 2, PROCESS_NATURE::CPU_BOUND);
    Process b = makeProcess(2, 0, 2, PROCESS_NATURE::CPU_BOUND);
    std::vector<Process*> processes = {&a, &b};

    cfs scheduler;
    std::vector<ProcessLog*> logs = scheduler.schedule(processes);

    REQUIRE(logs.size() == 4);
    REQUIRE(logs[0]->pid != logs[1]->pid);
    for (auto log : logs) delete log;
}

// --- RoundRobin::schedule -----------------------------------------------------

TEST_CASE("RoundRobin::schedule splits a long burst into quantum-sized slices", "[rr]") {
    Process p = makeProcess(1, 0, 5, PROCESS_NATURE::CPU_BOUND);
    std::vector<Process*> processes = {&p};

    RoundRobin scheduler(/*quantumMs=*/2);
    std::vector<ProcessLog*> logs = scheduler.schedule(processes);

    REQUIRE(logs.size() == 3);  // 2ms + 2ms + 1ms remainder
    for (auto log : logs) delete log;
}

TEST_CASE("Edge case: single process under RoundRobin completes in one slice if burst <= quantum",
          "[rr][edge]") {
    Process p = makeProcess(1, 0, 2, PROCESS_NATURE::CPU_BOUND);
    std::vector<Process*> processes = {&p};

    RoundRobin scheduler(/*quantumMs=*/5);
    std::vector<ProcessLog*> logs = scheduler.schedule(processes);

    REQUIRE(logs.size() == 1);
    delete logs[0];
}
