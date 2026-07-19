#ifndef IO_BOUND_PROCESS_EXECUTION_HPP
#define IO_BOUND_PROCESS_EXECUTION_HPP

#include "processService.hpp"
#include "queueService.hpp"
#include "cpuBoundProcessExecution.hpp"


constexpr int kIoCpuCreditMs = 1;


void handleIoBoundProcess(Process* process, int ioWaitTime, QueueService& q);

#endif // IO_BOUND_PROCESS_EXECUTION_HPP
