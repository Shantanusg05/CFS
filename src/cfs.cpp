#include <vector>
#include <algorithm>
#include "processService.hpp"
#include "processLog.hpp"
#include "queueService.hpp"
#include "cpuBoundProcessExecution.hpp"
#include "ioBoundProcessExecution.hpp"
#include "cfs.hpp"

namespace {
constexpr int kCpuTimeSliceMs = 1;
constexpr int kIoWaitTimeMs = 10;
constexpr long long kNsPerMs = 1'000'000LL;
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

    std::vector<Process*> ioWaitList;
    long long currentTime = 0;

    for (auto process : processList)
    {
        queue.push_element(process);
    }

    while (!queue.is_empty() || !ioWaitList.empty())
    {
        // Wake anyone whose I/O wait has completed by now -- the
        // discrete-event equivalent of an interrupt handler moving a
        // blocked task back onto the runqueue.
        for (size_t i = 0; i < ioWaitList.size(); )
        {
            if (ioWaitList[i]->ioWakeTime <= currentTime)
            {
                const long long sliceStart = currentTime;
                handleIoBoundProcess(ioWaitList[i], kIoWaitTimeMs, queue);
                currentTime += static_cast<long long>(kIoCpuCreditMs) * kNsPerMs;
                createProcessLog(logs, sliceStart, currentTime, ioWaitList[i]->pid);

                ioWaitList[i] = ioWaitList.back();
                ioWaitList.pop_back();
            }
            else
            {
                ++i;
            }
        }

        if (queue.is_empty())
        {
            if (ioWaitList.empty())
            {
                break;
            }
            // Nothing runnable right now -- jump the clock straight to the
            // next I/O completion instead of idling/polling.
            currentTime = std::min_element(ioWaitList.begin(), ioWaitList.end(),
                [](Process* a, Process* b) { return a->ioWakeTime < b->ioWakeTime; })->ioWakeTime;
            continue;
        }

        Process *process = queue.top_element();
        queue.pop_element();

        if (process->cpu_burst_time <= 0)
        {
            continue;
        }

        if (process->processNature == PROCESS_NATURE::CPU_BOUND)
        {
            const long long sliceStart = currentTime;
            executeCpuBoundProcess(process, kCpuTimeSliceMs, queue);
            currentTime += static_cast<long long>(kCpuTimeSliceMs) * kNsPerMs;
            createProcessLog(logs, sliceStart, currentTime, process->pid);
        }
        else
        {
            
            process->ioWakeTime = currentTime + static_cast<long long>(kIoWaitTimeMs) * kNsPerMs;
            ioWaitList.push_back(process);
        }
    }

    return logs;
}
