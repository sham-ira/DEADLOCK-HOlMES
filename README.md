
# Deadlock Holmes — Self-Healing OS Simulation

*"I observe, detect, and fix system problems in real time."*


A console-based C++ simulation that brings three classic operating system failure modes to life. Built as a course project for *CSE323 — Operating Systems*, this program auto-plays animated, tick-by-tick scenarios that show exactly what goes wrong inside an OS and how a self-healing system responds.

---

## Course Information

| Field       | Detail                                      |
|-------------|---------------------------------------------|
| Course      | CSE323 — Operating Systems                  |
| Institution | North South University                      |
| Semester    | Spring 2026                                 |
| Instructor  | Dr. Safat Siddiqui                                       |

---

## Demo Video

[![Watch the simulation demo]](https://your-video-link-here)

Replace the link above with your YouTube, Google Drive, or any hosted video URL.


---

## What It Does

Deadlock Holmes runs three acts automatically. No user input is needed between ticks — just sit back and watch the OS detective work in action.

---

### Act 1 — Memory Leak Detection and Self-Healing

*The problem:* A process requests RAM from the OS but never releases it after use. Every tick it holds more memory without giving any back. Left unchecked, this fills all available RAM and crashes the system.

*What you see:*
- All three processes start in a normal, stable state
- A memory leak is silently injected into Process 2
- Its memory bar grows by 12 MB every tick, turning yellow then red
- When it crosses the 90 MB danger threshold, Holmes fires the self-healing routine
- All leaked memory is forcibly reclaimed and the process resets to 10 MB
- The system returns to a stable state without any restart

P2    [####################] 91MB   Pr=2   LEAKING...
>> DANGER: P2 reached 91 MB, exceeding the 90 MB limit.
>> Self-healing: Forcing memory reclaim on P2. Resetting to 10 MB.
>> P2 is healthy again. Memory leak cleared.

---

### Act 2 — Deadlock Detection and Resolution

*The problem:* Process 1 holds Resource A and waits for Resource B. Process 2 holds Resource B and waits for Resource A. Neither can ever proceed. This circular wait freezes both processes permanently.

*What you see:*
- Two ticks of normal operation with no blocking
- P1 and P2 enter a circular resource dependency
- Both processes show BLOCKED status in the table
- Holmes detects the deadlock by counting mutually blocked processes
- The lowest-priority blocked process is preempted first
- All remaining blocked processes are released and resume

P1    [...................] 20MB   Pr=3   BLOCKED
P2    [...................] 20MB   Pr=3   BLOCKED
>> 2 processes are waiting on each other. Deadlock confirmed.
>> Preempting P2 (lowest priority) to break circular wait.
>> P1 unblocked and placed back in ready queue.

---

### Act 3 — CPU Starvation and Priority Aging

*The problem:* In priority scheduling, the CPU always goes to the highest-priority process. A very low-priority process may never get scheduled if higher-priority ones keep arriving — it waits indefinitely and makes no progress.

*What you see:*
- P1 starts at priority 5, P2 at priority 4, P3 at priority 1
- P1 runs every tick; P3 is always at the back of the queue
- After 3 consecutive skips, Holmes flags P3 as starving
- Priority aging gives P3 one extra priority point each time
- P3's priority climbs tick by tick until it finally wins the CPU

P3    [.....] 5MB   Pr=1   STARVING
>> P3 waited 3 ticks without CPU time. Starvation detected.
>> Priority aging: P3 priority raised from 1 to 2.

---

## Dashboard Guide

Every tick, the screen refreshes with a live dashboard. Here is how to read it.

============================================================
      DEADLOCK HOLMES  -  Self-Healing OS Simulation
============================================================
Act  : Act 1 - Memory Leak
Tick : 5

PROCESS TABLE
------------------------------------------------------------
PID   Memory (100 MB max)              Priority   Status
------------------------------------------------------------
P1    [##........] 20MB                Pr=3       RUNNING
P2    [########..] 67MB                Pr=2       LEAKING...
P3    [#.........] 5MB                 Pr=1       READY
------------------------------------------------------------

CPU Timeline  (oldest on left, newest on right)
  [ P1 ][ P1 ][ P1 ][ P1 ][ P1 ]

Total System Memory History  (0=low  9=high)
  [1][2][3][4][5]

HOLMES OBSERVES:
..........................................................
  >> P2 did not free memory this tick. 55 MB -> 67 MB.
..........................................................

| Element | Meaning |
|---|---|
| [######....]  42MB | Memory bar. More # means more RAM consumed. |
| [ P1 ][ P2 ][IDLE] | CPU Timeline. Which process ran each tick. |
| [0][3][6][9] | System memory load history. 0 = low, 9 = high. |
| >> text | Holmes explaining what just happened in plain English. |

*Status colours in terminal:*

| Status | Meaning |
|---|---|
| RUNNING | This process has the CPU this tick |
| LEAKING | Memory is growing abnormally each tick |
| STARVING | Process has been skipped too many times |
| BLOCKED | Process is stuck in a deadlock |
| DEAD | Process has been terminated |

---

## Prerequisites

### Software Required

| Tool | Minimum Version | Purpose |
|---|---|---|
| Windows OS | Windows 10 or later | Required — uses windows.h for console colouring |
| C++ Compiler | GCC 7+ or MSVC 2017+ | To compile the source file |
| MinGW-w64 | Any recent release | Recommended compiler for Windows (provides g++) |

### Installing MinGW (if you do not have a compiler)

1. Download MinGW-w64 from [winlibs.com](https://winlibs.com/) — pick the latest GCC release
2. Extract the folder and add its bin directory to your system PATH
3. Open a new Command Prompt and confirm with:

g++ --version

---

## How to Run

*Step 1 — Clone the repository*
git clone https://github.com/your-username/deadlock-holmes.git
cd deadlock-holmes

*Step 2 — Compile*
g++ -o deadlock_holmes deadlock_holmes.cpp -std=c++17

*Step 3 — Run*
deadlock_holmes.exe

The simulation starts immediately. Watch each act play out automatically. Press *ENTER* only when prompted between acts.

---

## Output Files

After the simulation finishes, two CSV log files are written to the same directory:

| File | Contents |
|---|---|
| cpu_history.csv | Which process ran (or was idle) each tick |
| memory_history.csv | Total system memory load each tick |

These can be opened in Excel or any spreadsheet tool to chart the simulation data.

---

## OS Concepts Covered

| Concept | Where It Appears |
|---|---|
| Priority scheduling | Every act — CPU always goes to highest priority first |
| Memory threshold monitoring | Act 1 — 90 MB limit triggers self-healing |
| Deadlock detection (circular wait) | Act 2 — counting mutually blocked processes |
| Resource preemption | Act 2 — lowest-priority victim selected to break cycle |
| Starvation | Act 3 — low-priority process never scheduled |
| Priority aging | Act 3 — wait counter triggers incremental priority boost |
| Self-healing | Acts 1 and 2 — system repairs itself without restart |


---

## License

This project was submitted as academic coursework at North South University.  
Free to use for educational and reference purposes
