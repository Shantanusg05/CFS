#include "RoundRobin.hpp"
#include <algorithm>
#include <queue>

namespace {
constexpr int kIoWaitTimeMs = 10;  // same convention as cfs.cpp
constexpr long long kNsPerMs = 1'000'000LL;
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

    std::vector<Process*> ioWaitList;
    long long currentTime = 0;

    for (auto process : processList) {
        runqueue.push(process);
    }

    while (!runqueue.empty() || !ioWaitList.empty()) {
        for (size_t i = 0; i < ioWaitList.size(); ) {
            if (ioWaitList[i]->ioWakeTime <= currentTime) {
                Process* process = ioWaitList[i];
                const long long sliceStart = currentTime;
                const int executedTime = std::min(quantumMs, process->cpu_burst_time);
                process->cpu_burst_time -= executedTime;
                currentTime += static_cast<long long>(executedTime) * kNsPerMs;
                createProcessLog(logs, sliceStart, currentTime, process->pid);

                if (process->cpu_burst_time > 0) {
                    runqueue.push(process);
                }

                ioWaitList[i] = ioWaitList.back();
                ioWaitList.pop_back();
            } else {
                ++i;
            }
        }

        if (runqueue.empty()) {
            if (ioWaitList.empty()) {
                break;
            }
            currentTime = std::min_element(ioWaitList.begin(), ioWaitList.end(),
                [](Process* a, Process* b) { return a->ioWakeTime < b->ioWakeTime; })->ioWakeTime;
            continue;
        }

        Process* process = runqueue.front();
        runqueue.pop();

        if (process->cpu_burst_time <= 0) {
            continue;
        }

        if (process->processNature == PROCESS_NATURE::IO_BOUND) {
            // Start an I/O wait -- comes off the runqueue entirely until
            // it wakes (handled at the top of the loop), instead of
            // blocking this thread.
            process->ioWakeTime = currentTime + static_cast<long long>(kIoWaitTimeMs) * kNsPerMs;
            ioWaitList.push_back(process);
            continue;
        }

        const long long sliceStart = currentTime;
        const int executedTime = std::min(quantumMs, process->cpu_burst_time);
        process->cpu_burst_time -= executedTime;
        currentTime += static_cast<long long>(executedTime) * kNsPerMs;

        createProcessLog(logs, sliceStart, currentTime, process->pid);

        if (process->cpu_burst_time > 0) {
            runqueue.push(process);
        }
    }

    return logs;
}
