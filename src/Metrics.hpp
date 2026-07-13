#ifndef METRICS_HPP
#define METRICS_HPP

#include <vector>
#include "processService.hpp"
#include "processLog.hpp"

struct SchedulerMetrics {
    double avgWaitTimeMs;
    double avgTurnaroundTimeMs;
    double avgResponseTimeMs;
    double vruntimeFairnessVariance;
};

// Computes summary statistics from a completed schedule's wall-clock logs.
// Does not take ownership of processList or logs — caller is still
// responsible for freeing them.
SchedulerMetrics computeMetrics(const std::vector<Process*>& processList,
                                 const std::vector<ProcessLog*>& logs);

#endif // METRICS_HPP
