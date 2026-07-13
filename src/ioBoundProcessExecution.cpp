#include "ioBoundProcessExecution.hpp"
#include <chrono>   // CHANGED: added explicitly — was relying on <thread> to
                     // transitively pull this in, which isn't guaranteed
#include <thread>

constexpr int NICE_0_LOAD = 1024;  // Standard Linux value

void handleIoBoundProcess(Process* process, int ioWaitTime, QueueService& q) {
    // Simulate IO wait
    std::this_thread::sleep_for(std::chrono::milliseconds(ioWaitTime));

    const double weight = weightFunction(process->priority);

    // CHANGED — sleeper fairness fix.
    //
    // Original behavior: process->vruntime += (ioWaitTime * NICE_0_LOAD) / weight;
    // This charged the I/O-bound process for the *entire* wait period as if
    // it had been running the whole time. In practice this meant an I/O-bound
    // process's vruntime would jump far ahead of every CPU-bound process in
    // the runqueue after every single I/O wait, so it would almost always
    // lose the next scheduling decision — i.e. I/O-bound processes were
    // effectively starved, which is the opposite of what real CFS does.
    //
    // Real CFS gives a waking ("sleeper") task a vruntime boost by clamping
    // its vruntime down to (approximately) the minimum vruntime currently in
    // the runqueue, instead of letting the sleep period inflate it. We
    // approximate that here: peek the current minimum vruntime in the
    // runqueue and clamp this process down to it if it's higher, before
    // charging only the small CPU-time portion actually executed after
    // waking.
    if (!q.is_empty()) {
        const long long minVruntimeInQueue = q.top_element()->vruntime;
        if (process->vruntime > minVruntimeInQueue) {
            process->vruntime = minVruntimeInQueue;
        }
    }

    // Deduct CPU burst time (1 time slice after IO)
    const int executedTime = 1;  // Same as CPU time slice
    process->cpu_burst_time -= executedTime;

    // CFS vruntime update for the CPU portion only (not the I/O wait itself)
    process->vruntime += (executedTime * NICE_0_LOAD) / weight;

    // Requeue if more burst remains
    if (process->cpu_burst_time > 0) {
        q.push_element(process);
    }
}
