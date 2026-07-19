#include "ioBoundProcessExecution.hpp"

constexpr int NICE_0_LOAD = 1024;  // Standard Linux value

void handleIoBoundProcess(Process* process, int ioWaitTime, QueueService& q) {
    
    (void)ioWaitTime;

    const double weight = weightFunction(process->priority);

    if (!q.is_empty()) {
        const long long minVruntimeInQueue = q.top_element()->vruntime;
        if (process->vruntime > minVruntimeInQueue) {
            process->vruntime = minVruntimeInQueue;
        }
    }

    process->cpu_burst_time -= kIoCpuCreditMs;
    process->vruntime += (kIoCpuCreditMs * NICE_0_LOAD) / weight;

    if (process->cpu_burst_time > 0) {
        q.push_element(process);
    }
}
