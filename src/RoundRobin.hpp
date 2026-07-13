#ifndef ROUND_ROBIN_HPP
#define ROUND_ROBIN_HPP

#include <vector>
#include <string>
#include "Scheduler.hpp"
#include "processService.hpp"
#include "processLog.hpp"

// A second scheduling policy implementing the same Scheduler interface as
// cfs, so they can be run on the same workload and compared. Unlike cfs,
// this uses a plain FIFO queue and a fixed time quantum instead of a
// vruntime-ordered min-heap — no notion of weighted fairness by priority.
class RoundRobin : public Scheduler {
private:
    int quantumMs;
    void createProcessLog(std::vector<ProcessLog*>& logs, long long startTime, long long endTime, int pid);

public:
    explicit RoundRobin(int quantumMs = 2);
    std::vector<ProcessLog*> schedule(std::vector<Process*> processList) override;
    std::string name() const override { return "RoundRobin"; }
};

#endif // ROUND_ROBIN_HPP
