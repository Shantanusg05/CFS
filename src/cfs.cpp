#include <vector>
#include <chrono>
#include "processService.hpp"
#include "processLog.hpp"
#include "queueService.hpp"
#include "cpuBoundProcessExecution.hpp"
#include "ioBoundProcessExecution.hpp"
#include "cfs.hpp"

namespace {
// CHANGED: pulled the magic numbers 1 and 10 out of schedule() into named
// constants — same values, just named, so the meaning isn't buried in the
// call site.
constexpr int kCpuTimeSliceMs = 1;
constexpr int kIoWaitTimeMs = 10;
}  // namespace

void cfs::createProcessLog(std::vector<ProcessLog *> &logs, long long startTime, long long endTime, int pid)
{
    ProcessLog *p = new ProcessLog();
    p->pid = pid;
    p->startTime = startTime;
    p->endTime = endTime;
    logs.push_back(p);
}

std::vector<ProcessLog *> cfs::schedule(std::vector<Process *> processList)
{
    QueueService queue;
    std::vector<ProcessLog *> logs;

    for (auto process : processList)
    {
        queue.push_element(process);
    }

    while (!queue.is_empty())
    {
        Process *process = queue.top_element();
        queue.pop_element();

        // CHANGED — fix: a process pushed with cpu_burst_time <= 0 (e.g. a
        // zero-burst input) would otherwise still get dispatched once and
        // logged with a no-op (zero-work) entry, since
        // executeCpuBoundProcess only prevents *requeueing*, not the
        // initial dispatch+log. Skip it outright instead.
        if (process->cpu_burst_time <= 0)
        {
            continue;
        }

        auto startTime = std::chrono::steady_clock::now();

        if (process->processNature == PROCESS_NATURE::CPU_BOUND)
        {
            executeCpuBoundProcess(process, kCpuTimeSliceMs, queue);
        }
        else
        {
            handleIoBoundProcess(process, kIoWaitTimeMs, queue);
        }

        // Record end time in nanoseconds
        auto endTime = std::chrono::steady_clock::now();

        long long startTimeMs = std::chrono::duration_cast<std::chrono::nanoseconds>(startTime.time_since_epoch()).count();
        long long endTimeMs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime.time_since_epoch()).count();

        createProcessLog(logs, startTimeMs, endTimeMs, process->pid);
    }

    return logs;
}
