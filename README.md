v# CFS Scheduler Simulation

A C++ simulation of two CPU scheduling policies — the Linux **Completely
Fair Scheduler (CFS)** and **Round Robin** — implemented behind a common
`Scheduler` interface so they can be run and compared on the same
synthetic workload of CPU-bound and I/O-bound processes.

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Build and Run](#build-and-run)
4. [How Scheduling Works](#how-scheduling-works)
5. [Design Decisions and Known Simplifications](#design-decisions-and-known-simplifications)
6. [Bug Fixes Made During This Pass](#bug-fixes-made-during-this-pass)
7. [Testing](#testing)
8. [Metrics](#metrics)
9. [Future Work](#future-work)

---

## Overview

CFS schedules processes by **virtual runtime (`vruntime`)**: the process
with the lowest `vruntime` always runs next, and higher-priority processes
accumulate `vruntime` more slowly (via a priority-derived `weight`), so
they get scheduled more often without ever fully starving lower-priority
processes. This project simulates that core mechanism — including the less
commonly implemented **sleeper fairness** nuance for I/O-bound processes —
and additionally implements Round Robin as a second policy for comparison.

This is a **user-space simulation**, not a kernel scheduler: there is no
real concurrent execution, no actual context switching, and no multi-core
support. The goal is to demonstrate the scheduling *algorithm*, not to
build an OS component.

## Architecture

```
main.cpp                       — entry point; selects a scheduler, runs it, writes output
src/
  Scheduler.hpp                 — abstract interface implemented by every policy
  cfs.hpp / cfs.cpp             — CFS: vruntime-ordered min-heap runqueue
  RoundRobin.hpp / .cpp         — Round Robin: FIFO queue + fixed time quantum
  queueService.hpp / .cpp       — min-heap (priority_queue) runqueue used by CFS
  cpuBoundProcessExecution.*    — weightFunction() + CPU-bound execution step
  ioBoundProcessExecution.*     — I/O-bound execution step, including sleeper fairness
  processService.hpp / .cpp     — Process struct + JSON loader
  processLog.hpp                — ProcessLog struct (per-slice execution record)
  Metrics.hpp / .cpp            — turnaround/wait/response time + fairness variance
tests/
  test_scheduler.cpp            — Catch2 unit tests
resources/
  process.json                  — input workload (sample provided; replace with your own)
```

`main.cpp` depends only on the `Scheduler` interface, not on `cfs` or
`RoundRobin` directly. Adding a third policy means writing one new class
that implements `Scheduler`; nothing else in the call chain changes.

## Build and Run

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make

./cfs_scheduler cfs    # run CFS
./cfs_scheduler rr     # run Round Robin
ctest --output-on-failure   # run unit tests
```

Before building, copy your `json.hpp` (nlohmann/json single-header) into
the `nlohmann/` folder at the project root — see
`nlohmann/PLACE_JSON_HPP_HERE.txt`.

Each run writes `process_schedule.csv` (one row per executed time slice:
`pid,start_time,end_time`) and prints summary metrics to stdout, so `cfs`
and `rr` runs on the same `resources/process.json` can be compared
directly.

## How Scheduling Works

**Weight:** `weight = NICE_0_LOAD / (priority + 1)`, where `NICE_0_LOAD =
1024` (the same constant the Linux kernel uses). Higher priority →
larger weight.

**Vruntime growth (CPU-bound):** `vruntime += (executed_time *
NICE_0_LOAD) / weight`. A higher weight means a smaller increment per
unit of execution — i.e. the process "ages" more slowly and gets
rescheduled sooner relative to others.

**Runqueue:** a min-heap (`std::priority_queue` with a custom comparator)
ordered by `vruntime`. The process with the smallest `vruntime` is always
selected next — this is the core fairness mechanism.

**I/O-bound processes and sleeper fairness:** when a process wakes up
from simulated I/O wait, it does **not** get charged `vruntime` for the
entire wait period. If it did, every I/O-bound process would fall behind
every CPU-bound process in the runqueue after every wait and effectively
starve — which is the opposite of the fairness CFS is supposed to
provide. Instead, a waking process's `vruntime` is clamped down to
(approximately) the current minimum `vruntime` in the runqueue before a
small charge is added for the CPU time it actually executes after waking.
This mirrors the real kernel's sleeper-fairness behavior, simplified.

**Round Robin (for comparison):** a plain FIFO queue and a fixed time
quantum (default 2ms), with no notion of weighted fairness — every
process gets equal-sized turns regardless of priority. Comparing its
output metrics against CFS on the same workload demonstrates the tradeoff
CFS is solving for: throughput/simplicity (RR) vs. priority-weighted
fairness (CFS).

## Design Decisions and Known Simplifications

- **Why a min-heap instead of a red-black tree?** CFS needs O(log n)
  access to the lowest-vruntime task on every reschedule, which a binary
  heap provides directly. The real kernel uses a red-black tree because it
  also needs efficient O(log n) *arbitrary* removal — needed when a task
  is migrated between per-CPU runqueues. Since this simulation has a
  single runqueue (no multi-core), the heap is sufficient and simpler.
- **Real wall-clock timing, not simulated discrete time.** `ProcessLog`
  entries are stamped with actual `std::chrono::steady_clock` timestamps
  around each scheduling decision, and I/O wait is a real
  `std::this_thread::sleep_for`. This means CPU-bound execution (no
  artificial delay) takes near-zero real time per slice, while I/O-bound
  waits take real milliseconds. **Practical effect on metrics:** "wait
  time" and "turnaround time" mostly reflect real I/O wait and scheduling
  overhead, not a fully simulated workload completion time — this is a
  deliberate property of how the original scheduling loop measures time,
  not a bug, but it's worth understanding before reading too much into the
  absolute numbers.
- **Sleeper fairness is approximated**, not implemented to kernel
  precision (the real implementation also accounts for
  `sched_min_granularity` and other tunables this simulation doesn't
  model).
- **What's explicitly out of scope:** multi-core scheduling and load
  balancing, preemption granularity, cgroup/group scheduling, and real OS
  context switching. This project demonstrates the scheduling *algorithm*
  at the single-runqueue level, not a kernel subsystem.


## Testing

Unit tests use Catch2 (fetched via CMake `FetchContent`, no manual
install needed) and cover:

- the weight formula directly,
- CPU-bound execution (burst decrement, vruntime increase, requeue vs.
  not-requeue),
- the sleeper-fairness clamp specifically — including a negative case
  (a process already below the runqueue minimum should not be raised),
- min-heap dispatch ordering,
- two `cfs::schedule` integration tests (slice count, equal-priority
  interleaving),
- Round Robin quantum slicing.

Tests call the underlying functions (`weightFunction`,
`executeCpuBoundProcess`, `handleIoBoundProcess`, `QueueService`) directly
rather than asserting on `schedule()`'s wall-clock timestamps, since those
aren't deterministic between runs — testing the logic in isolation is both
more reliable and arguably better practice than asserting on real-time
output.

## Metrics

Each run prints:

- **Average wait time** — time a process spent in the runqueue not
  executing, relative to total turnaround.
- **Average turnaround time** — time from first dispatch to final
  completion.
- **Average response time** — time from simulation start to a process's
  first dispatch.
- **Vruntime fairness variance** — spread of final `vruntime` values
  across all processes; lower means the scheduler kept processes' virtual
  runtimes closer together, which is the fairness property CFS is
  explicitly optimizing for. (Not meaningful for Round Robin, which
  doesn't track `vruntime`.)

## Future Work

- Multi-core simulation with per-core runqueues and a load-balancing
  policy (e.g. migrate to the least-loaded core) — a natural next step,
  intentionally left out of this pass to keep the change set reviewable.
- A third policy (e.g. Multi-Level Feedback Queue) for a richer
  comparison.
- Configurable I/O wait time and time quantum via `process.json` /
  CLI flags instead of compile-time constants.
