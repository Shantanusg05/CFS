#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <string>
#include <vector>
#include "processService.hpp"
#include "processLog.hpp"

// Common interface every scheduling policy implements. main.cpp depends
// only on Scheduler, never on cfs or RoundRobin directly — that's what
// makes this a pluggable scheduler framework rather than one fixed
// algorithm. Adding a third policy later means writing a new class that
// implements this interface; nothing else needs to change.
class Scheduler {
public:
    virtual ~Scheduler() = default;
    virtual std::vector<ProcessLog*> schedule(std::vector<Process*> processList) = 0;
    virtual std::string name() const = 0;
};

#endif // SCHEDULER_HPP
