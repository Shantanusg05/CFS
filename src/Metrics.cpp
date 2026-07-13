#include "Metrics.hpp"
#include <unordered_map>

SchedulerMetrics computeMetrics(const std::vector<Process*>& processList,
                                 const std::vector<ProcessLog*>& logs) {
    if (logs.empty()) {
        return {0.0, 0.0, 0.0, 0.0};
    }

    // Earliest timestamp across all log entries = "t = 0" for this run,
    // since ProcessLog stores raw steady_clock epoch nanoseconds, not
    // simulation-relative time.
    long long simulationStart = logs.front()->startTime;
    for (auto log : logs) {
        if (log->startTime < simulationStart) simulationStart = log->startTime;
    }

    std::unordered_map<int, long long> firstStart;
    std::unordered_map<int, long long> lastEnd;
    std::unordered_map<int, long long> busyTimeNs;

    for (auto log : logs) {
        if (firstStart.find(log->pid) == firstStart.end()) {
            firstStart[log->pid] = log->startTime;
        }
        lastEnd[log->pid] = log->endTime;
        busyTimeNs[log->pid] += (log->endTime - log->startTime);
    }

    double sumWaitMs = 0.0, sumTurnaroundMs = 0.0, sumResponseMs = 0.0;
    const int n = static_cast<int>(firstStart.size());

    for (const auto& entry : firstStart) {
        const int pid = entry.first;
        const long long start = entry.second;

        const double turnaroundMs = static_cast<double>(lastEnd[pid] - simulationStart) / 1e6;
        const double busyMs = static_cast<double>(busyTimeNs[pid]) / 1e6;
        const double waitMs = turnaroundMs - busyMs;
        const double responseMs = static_cast<double>(start - simulationStart) / 1e6;

        sumTurnaroundMs += turnaroundMs;
        sumWaitMs += waitMs;
        sumResponseMs += responseMs;
    }

    double meanVruntime = 0.0;
    for (auto process : processList) {
        meanVruntime += static_cast<double>(process->vruntime);
    }
    meanVruntime /= processList.empty() ? 1 : static_cast<double>(processList.size());

    double variance = 0.0;
    for (auto process : processList) {
        const double diff = static_cast<double>(process->vruntime) - meanVruntime;
        variance += diff * diff;
    }
    variance /= processList.empty() ? 1 : static_cast<double>(processList.size());

    return {sumWaitMs / n, sumTurnaroundMs / n, sumResponseMs / n, variance};
}
