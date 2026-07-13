#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include "src/processService.hpp"
#include "src/processLog.hpp"
#include "src/Scheduler.hpp"
#include "src/cfs.hpp"
#include "src/RoundRobin.hpp"
#include "src/Metrics.hpp"

int main(int argc, char* argv[]) {
    // Usage: ./cfs_scheduler [cfs|rr]
    const std::string algo = (argc > 1) ? argv[1] : "cfs";

    std::vector<Process*> processes = getProcessFromJson("../resources/process.json");

    std::unique_ptr<Scheduler> scheduler;
    if (algo == "rr") {
        scheduler = std::make_unique<RoundRobin>(2);
    } else {
        scheduler = std::make_unique<cfs>();
    }

    std::vector<ProcessLog*> logs = scheduler->schedule(processes);

    std::ofstream outFile("../process_schedule.csv");
    outFile << "pid,start_time,end_time" << std::endl;
    for (auto processLog : logs) {
        outFile << processLog->pid << ","
                << processLog->startTime << ","
                << processLog->endTime << std::endl;
    }
    outFile.close();

    const SchedulerMetrics metrics = computeMetrics(processes, logs);
    std::cout << "Scheduler: " << scheduler->name() << "\n"
              << "Avg wait time:               " << metrics.avgWaitTimeMs << " ms\n"
              << "Avg turnaround time:         " << metrics.avgTurnaroundTimeMs << " ms\n"
              << "Avg response time:           " << metrics.avgResponseTimeMs << " ms\n"
              << "Vruntime fairness variance:  " << metrics.vruntimeFairnessVariance << "\n";

    // CHANGED: the original code never freed the Process* objects allocated
    // in getProcessFromJson, nor the ProcessLog* objects allocated in
    // cfs::createProcessLog — every run leaked memory. The rest of the
    // codebase uses raw pointers throughout (QueueService, cfs, RoundRobin),
    // so rather than rewriting everything to smart pointers, clean up
    // explicitly here where ownership ends.
    for (auto process : processes) {
        delete process;
    }
    for (auto processLog : logs) {
        delete processLog;
    }

    return 0;
}

// Higher-priority processes "age" slower in vruntime, so they get scheduled more frequently.
