#ifndef CFS_HPP
#define CFS_HPP

#include <vector>
#include <ctime>
#include "processService.hpp"
#include "processLog.hpp"
#include "queueService.hpp"
#include "cpuBoundProcessExecution.hpp"
#include "ioBoundProcessExecution.hpp"
#include "Scheduler.hpp"  // CHANGED: added

class cfs : public Scheduler {  // CHANGED: now implements Scheduler
private:
    void createProcessLog(std::vector<ProcessLog*> &logs, long long startTime, long long endTime, int pid);

public:
    std::vector<ProcessLog*> schedule(std::vector<Process*> processList) override;  // CHANGED: added override
    std::string name() const override { return "CFS"; }  // CHANGED: added
};

#endif // CFS_HPP
