#include "RoundRobin.hpp"
#include <algorithm>
#include <chrono>
#include <queue>
#include <thread>

namespace {
constexpr int kIoWaitTimeMs = 10;  // same convention as cfs.cpp
}  // namespace

RoundRobin::RoundRobin(int quantumMs) : quantumMs(quantumMs) {}

void RoundRobin::createProcessLog(std::vector<ProcessLog*>& logs, long long startTime, long long endTime, int pid) {
    ProcessLog* p = new ProcessLog();
    p->pid = pid;
    p->startTime = startTime;
    p->endTime = endTime;
    logs.push_back(p);
}

std::vector<ProcessLog*> RoundRobin::schedule(std::vector<Process*> processList) {
    std::queue<Process*> runqueue;
    std::vector<ProcessLog*> logs;

    for (auto process : processList) {
        runqueue.push(process);
    }

    while (!runqueue.empty()) {
        Process* process = runqueue.front();
        runqueue.pop();

        // CHANGED — same guard as cfs.cpp: skip a process with no
        // remaining burst time instead of logging a no-op slice.
        if (process->cpu_burst_time <= 0) {
            continue;
        }

        auto startTime = std::chrono::steady_clock::now();

        if (process->processNature == PROCESS_NATURE::IO_BOUND) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kIoWaitTimeMs));
        }

        const int executedTime = std::min(quantumMs, process->cpu_burst_time);
        process->cpu_burst_time -= executedTime;

        auto endTime = std::chrono::steady_clock::now();

        long long startTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(startTime.time_since_epoch()).count();
        long long endTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime.time_since_epoch()).count();

        createProcessLog(logs, startTimeNs, endTimeNs, process->pid);

        if (process->cpu_burst_time > 0) {
            runqueue.push(process);
        }
    }

    return logs;
}
