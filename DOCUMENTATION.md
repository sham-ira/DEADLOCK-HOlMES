# Deadlock Holmes — Developer Documentation

> Complete technical reference for understanding, extending, and contributing to the Deadlock Holmes self-healing OS simulation.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Repository Structure](#2-repository-structure)
3. [Architecture Deep Dive](#3-architecture-deep-dive)
4. [Data Structures](#4-data-structures)
5. [Core Modules](#5-core-modules)
6. [Simulation Flow](#6-simulation-flow)
7. [Self-Healing Algorithms](#7-self-healing-algorithms)
8. [Console Rendering System](#8-console-rendering-system)
9. [Configuration Reference](#9-configuration-reference)
10. [Extending the Simulation](#10-extending-the-simulation)
11. [Known Limitations](#11-known-limitations)
12. [Glossary](#12-glossary)

---

## 1. Project Overview

Deadlock Holmes is a single-file C++ console application that simulates a minimal operating system scheduler with built-in fault detection and self-healing. It runs three automated acts that each demonstrate a different OS failure mode, then repairs each fault without stopping execution.

### Design Principles

- **Automatic execution.** No user interaction is required during a scenario. The simulation advances on its own tick by tick.
- **Plain-English narration.** Every event is described in a Holmes observation line so the output is self-documenting.
- **ASCII-only output.** All visual elements use characters from the standard ASCII range to guarantee compatibility with the Windows Command Prompt.
- **Single source file.** The entire simulation fits in one `.cpp` file to keep it easy to study, submit, and share.

### What a "Tick" Means

A tick is one complete simulation cycle. In each tick the system:

1. Grows memory for any leaking processes
2. Checks for and resolves deadlocks
3. Runs the scheduler
4. Updates the history deques
5. Redraws the dashboard

The real-world clock duration of a tick is controlled by the `Sleep()` call at the end of each `tick()` invocation.

---

## 2. Repository Structure

```
deadlock-holmes/
|
|-- deadlock_holmes.cpp       Single-file simulation source
|-- README.md                 User-facing project overview
|-- DOCUMENTATION.md          This file — developer reference
|-- cpu_history.csv           Generated on exit (CPU log per tick)
|-- memory_history.csv        Generated on exit (memory log per tick)
```

All output files are written to the same directory as the executable.

---

## 3. Architecture Deep Dive

The program is organised into seven layers. Each layer calls downward only — no layer reaches upward to call something above it.

```
+----------------------------------------------------------+
|                  ACT ORCHESTRATORS                       |
|   actMemoryLeak()   actDeadlock()   actStarvation()      |
+----------------------------------------------------------+
                          |
+----------------------------------------------------------+
|                    TICK ENGINE                           |
|                     tick()                               |
+----------------------------------------------------------+
          |                |                |
+-----------------+ +-------------+ +------------------+
| LEAK DETECTOR   | | DEADLOCK    | | SCHEDULER        |
| (inside tick()) | | DETECTOR    | | schedule()       |
|                 | | (inside     | |                  |
| Threshold check | |  tick())    | | Priority sort    |
| Self-heal reset | | deadCount   | | CPU assignment   |
|                 | | Preemption  | | Starvation aging |
+-----------------+ +-------------+ +------------------+
                          |
+----------------------------------------------------------+
|                  RENDER ENGINE                           |
|  renderDashboard() -> renderProcessTable()               |
|                       renderCpuTimeline()                |
|                       renderMemoryGraph()                |
|                       renderObservations()               |
+----------------------------------------------------------+
                          |
+----------------------------------------------------------+
|               CONSOLE / ASCII UTILITIES                  |
|   setColor()   clearScreen()   wait()   makeBar()        |
+----------------------------------------------------------+
                          |
+----------------------------------------------------------+
|                   CSV EXPORTER                           |
|                   exportCSV()                            |
+----------------------------------------------------------+
```

---

## 4. Data Structures

### 4.1 The Process Struct

Every simulated process is represented by one instance of this struct.

```cpp
struct Process {
    int    id;           // Unique identifier: 1, 2, or 3
    int    memory;       // Current RAM usage in MB
    int    memoryMax;    // Danger threshold — healing fires when memory >= memoryMax
    int    priority;     // Scheduler priority: higher number = scheduled first
    int    waitTime;     // Consecutive ticks this process has waited without CPU time
    int    waitingFor;   // ID of the process this one is blocked on; -1 if not blocked
    bool   memoryLeak;   // true = this process will grow memory each tick
    bool   active;       // false = process has been terminated; excluded from all logic
    bool   waiting;      // true = process is blocked (deadlock state)
    string status;       // Human-readable status set by the scheduler each tick
};
```

**Status values written by the scheduler:**

| Value | Meaning |
|---|---|
| `RUNNING` | This process received the CPU this tick |
| `READY` | Runnable but waiting for CPU |
| `STARVING` | Wait time crossed the starvation threshold; aging was applied |
| `BLOCKED` | Process is in the waiting state (deadlock scenario) |
| `DEAD` | Process is not active |

### 4.2 History Deques

Two global deques record the last 15 ticks of system state for the timeline displays.

```cpp
deque<int> cpuHistory;   // ID of process that ran each tick; -1 = idle
deque<int> memHistory;   // Total system memory (sum of all active process memory) each tick
```

Both deques cap at 15 entries. The oldest entry is evicted with `pop_front()` when a new entry pushes the size past the limit.

---

## 5. Core Modules

### 5.1 `tick()` — The Simulation Engine

```cpp
void tick(vector<Process>& P, int tickNo, const string& actName, vector<string>& obs);
```

This is the central function of the simulation. It executes one complete time step in five phases. The `obs` vector collects observation strings during the tick and is cleared after rendering.

**Phase sequence inside `tick()`:**

```
1. For each process:
       if memoryLeak → p.memory += 12
       if p.memory >= p.memoryMax → self-heal (reset + clear flag)

2. Count how many processes have p.waiting == true
   if deadCount >= 2 → find lowest-priority waiting process, preempt it,
                        release all other waiting processes

3. Call schedule(P, obs)
   → sorts by priority, assigns CPU, detects starvation, applies aging

4. Push running ID to cpuHistory; push totalMem to memHistory
   Evict oldest if size > 15

5. Call renderDashboard(P, actName, tickNo, obs)
   obs.clear()
```

---

### 5.2 `schedule()` — The Scheduler

```cpp
int schedule(vector<Process>& P, vector<string>& obs);
```

Returns the ID of the process that ran this tick, or `-1` if no process was runnable.

**Algorithm:**

```
stable_sort(P, descending by priority)

cpuUsed = false

for each process p in sorted order:
    if not active or waiting:
        set status = BLOCKED or DEAD
        skip

    if cpuUsed == false:
        p.status   = RUNNING
        p.waitTime = 0
        running    = p.id
        cpuUsed    = true

    else:
        p.waitTime++
        p.status = READY

        if p.waitTime >= 3:
            p.status = STARVING
            p.priority++
            p.waitTime = 0
            log aging event to obs

return running
```

`stable_sort` is used rather than `sort` so that processes with equal priority retain their original relative order, which prevents unnecessary oscillation when aging brings two processes to the same priority level.

---

### 5.3 `renderDashboard()` — The Render Engine

```cpp
void renderDashboard(const vector<Process>& P,
                     const string& actName,
                     int tick,
                     const vector<string>& obs);
```

Clears the screen and redraws the complete console UI in five sections:

| Section | Function called |
|---|---|
| Header (act name, tick number) | `renderHeader()` |
| Process table with memory bars | `renderProcessTable()` |
| CPU timeline | `renderCpuTimeline()` |
| Memory load history | `renderMemoryGraph()` |
| Holmes observations | `renderObservations()` |

**Color coding in `renderProcessTable()`:**

| Condition | Windows console color code | Meaning |
|---|---|---|
| `!p.active` | 8 (dark grey) | Process is dead |
| `p.memory > 70` | 12 (red) | High memory usage |
| `p.memory > 40` | 14 (yellow) | Medium memory usage |
| otherwise | 10 (green) | Healthy memory level |
| `p.waiting` | 12 (red) | Deadlocked |
| `p.memoryLeak` | 14 (yellow) | Leaking |
| `RUNNING` | 10 (green) | Active on CPU |
| `STARVING` | 13 (magenta) | Starvation detected |

---

### 5.4 `makeBar()` — The ASCII Bar Builder

```cpp
string makeBar(int value, int maxVal, int barWidth, const string& unit = "");
```

Returns a fixed-width bar string. Example output: `[######..........] 42MB`

- `value` — current amount
- `maxVal` — maximum possible amount (sets the scale)
- `barWidth` — total number of characters inside the brackets
- `unit` — string appended after the number (e.g., `"MB"`)

The number of `#` characters is `(value * barWidth) / maxVal`, clamped to `[0, barWidth]`.

---

### 5.5 `exportCSV()` — The Log Exporter

```cpp
void exportCSV();
```

Writes two files to the current working directory after all acts complete.

**`cpu_history.csv`**

```
Tick,ProcessID
1,1
2,1
3,-1
...
```

ProcessID `-1` means the CPU was idle that tick.

**`memory_history.csv`**

```
Tick,TotalMemoryMB
1,30
2,42
3,66
...
```

TotalMemoryMB is the sum of `p.memory` across all active processes at the time the tick's history entry was pushed.

---

## 6. Simulation Flow

### Act Sequence

```
splashScreen()
    |
    v
actMemoryLeak()
    |--- 2 ticks normal operation
    |--- inject leak into P2
    |--- run until self-heal fires
    |--- 2 ticks post-heal stable
    |
    v
actDeadlock()
    |--- 2 ticks normal operation
    |--- inject circular wait (P1 <-> P2)
    |--- run until deadlock detected and resolved
    |--- 2 ticks post-resolution stable
    |
    v
actStarvation()
    |--- 14 ticks total
    |--- P3 starves, ages, eventually runs
    |
    v
exportCSV()
    |
    v
Exit
```

### Tick Timing

| Act | Sleep per tick |
|---|---|
| Memory Leak (normal ticks) | 1600 ms |
| Memory Leak (leaking ticks) | 1300 ms |
| Deadlock (normal ticks) | 1600 ms |
| Deadlock (blocked ticks) | 1300 ms |
| Starvation | 1100 ms |

Starvation uses the shortest delay because it runs for the most ticks and the events are less dramatic per tick.

---

## 7. Self-Healing Algorithms

### 7.1 Memory Leak Healing

```
Trigger:   p.memory >= p.memoryMax
Response:  p.memory = 10
           p.memoryLeak = false
Effect:    Process continues running. No restart. No termination.
```

The threshold `p.memoryMax` defaults to 90 for all processes. The reset value of 10 MB represents a clean baseline. The process's `memoryLeak` flag is cleared so growth stops immediately.

### 7.2 Deadlock Resolution

```
Trigger:   deadCount >= 2  (count of active processes with p.waiting == true)

Step 1:    Find victim = process in blocked set with lowest priority
Step 2:    victim.waiting = false
           victim.waitingFor = -1
Step 3:    For all remaining blocked processes:
               p.waiting = false
               p.waitingFor = -1
```

The victim selection strategy (lowest priority) follows the principle of least harm: preempting the process that would contribute least to forward progress if it ran next. All other blocked processes are freed in the same tick, meaning deadlock resolution always completes in exactly one tick.

### 7.3 Priority Aging

```
Trigger:   p.waitTime >= 3  (consecutive ticks without CPU time)
Response:  p.priority += 1
           p.waitTime = 0
           p.status = "STARVING"
```

Aging resets the wait counter so the next increment requires another three consecutive skips. This creates a gradual, metered priority increase rather than an instant jump. The process will eventually reach a priority level at which `stable_sort` places it ahead of all processes that were previously outrunning it.

---

## 8. Console Rendering System

### Color Codes (Windows Console)

The simulation uses `SetConsoleTextAttribute()` with the following codes:

| Code | Color | Usage |
|---|---|---|
| 7 | White (default) | Normal text, table borders |
| 8 | Dark grey | Dead processes, dim elements |
| 10 | Green | Healthy memory, RUNNING status |
| 11 | Cyan | Headers, timeline labels |
| 12 | Red | High memory, BLOCKED status, danger alerts |
| 13 | Magenta | STARVING status |
| 14 | Yellow | Medium memory, LEAKING status, observation arrows |

### Screen Refresh Approach

The simulation calls `system("cls")` at the start of each `renderDashboard()` call. This clears the entire console buffer before redrawing. The `Sleep()` call between ticks provides enough time for the redraw to complete before the next clear fires, minimising perceived flicker.

An alternative using `SetConsoleCursorPosition()` to overwrite in place was considered but rejected because the number of observation lines varies each tick, making fixed-position overwrite unreliable.

---

## 9. Configuration Reference

All tunable values are hardcoded as process field initializers inside each act function. To change simulation behaviour, edit the `vector<Process> P` declaration at the start of the relevant act.

### Process Initialization Fields

```cpp
{id, memory, memoryMax, priority, waitTime, waitingFor, memoryLeak, active, waiting, status}
```

| Field | Default | Effect of changing |
|---|---|---|
| `memory` | 5-20 | Starting RAM usage. Higher values make the process appear loaded from the start. |
| `memoryMax` | 90 | Leak healing threshold in MB. Lower = heals sooner. |
| `priority` | 1-5 | Scheduling priority. Larger gap between processes creates more visible starvation. |
| `memoryLeak` | false | Set to true to start a process already leaking. |

### Leak Growth Rate

Inside `tick()`, the leak increment is hardcoded:

```cpp
p.memory += 12;
```

Change `12` to adjust how fast leaked memory grows per tick. At 12 MB/tick with a 90 MB threshold, healing fires after approximately 6-7 ticks of leaking, assuming the process starts at 15 MB.

### Starvation Threshold

Inside `schedule()`:

```cpp
if (p.waitTime >= 3)
```

Change `3` to require more or fewer consecutive skips before aging fires. A value of `1` makes aging fire every single tick a process does not run.

---

## 10. Extending the Simulation

### Adding a New Act

1. Create a new function following the existing pattern:

```cpp
void actMyNewScenario() {

    clearScreen();
    drawBox("ACT 4 OF 4  -  MY SCENARIO TITLE", { /* description lines */ }, COLOR_CODE);
    pressEnterToContinue();

    vector<Process> P = { /* initial state */ };
    vector<string> obs;
    int t = 1;

    // Pre-fault ticks
    for (int i = 0; i < 2; i++, t++) {
        obs.push_back("Normal operation.");
        tick(P, t, "Act 4 - My Scenario", obs);
        wait(1600);
    }

    // Inject fault
    // ... modify P fields here ...

    // Run until resolved
    for (int i = 0; i < 10; i++, t++) {
        tick(P, t, "Act 4 - My Scenario", obs);
        wait(1300);
        if (/* resolved condition */) break;
    }

    drawBox("ACT 4 COMPLETE", { /* summary lines */ }, COLOR_CODE);
    pressEnterToContinue();
}
```

2. Call your new function in `main()` after `actStarvation()`.

### Adding a New Fault Type

To add a fault type that `tick()` handles automatically:

1. Add a new `bool` flag to the `Process` struct (e.g., `bool cpuSpike`)
2. Add a detection block inside `tick()`, before the `schedule()` call
3. Write the self-heal logic inside that block
4. Add observations to `obs` to narrate what is happening

### Adding More Processes

The simulation supports any number of processes. Add entries to the `vector<Process> P` initializer:

```cpp
vector<Process> P = {
    {1, 10, 90, 5, 0, -1, false, true, false, "READY"},
    {2, 10, 90, 4, 0, -1, false, true, false, "READY"},
    {3, 10, 90, 3, 0, -1, false, true, false, "READY"},
    {4, 10, 90, 2, 0, -1, false, true, false, "READY"},  // new
    {5, 10, 90, 1, 0, -1, false, true, false, "READY"},  // new
};
```

The process table and all detection logic will automatically scale to accommodate the additional entries.

---

## 11. Known Limitations

| Limitation | Detail |
|---|---|
| Windows only | Uses `windows.h` for `SetConsoleTextAttribute()` and `Sleep()`. Will not compile on Linux or macOS without a compatibility layer. |
| Single CPU | The scheduler assigns the CPU to exactly one process per tick. Multi-core scheduling is not modelled. |
| No real resources | Resource allocation is simulated only through boolean flags. There are no actual mutexes, semaphores, or memory allocators involved. |
| Fixed process count | Process IDs and the vector size are set at act initialisation. Processes cannot be created dynamically during a running act. |
| Deadlock limited to 2 processes | The injection always targets P1 and P2. The detection correctly handles any count, but the demonstration always uses exactly two. |
| No inter-tick persistence | Each act creates a fresh `vector<Process>`. State does not carry over between acts. |

---

## 12. Glossary

| Term | Definition as used in this project |
|---|---|
| Tick | One complete simulation cycle: leak growth, deadlock check, scheduling, history update, render |
| Memory leak | A process that increments `p.memory` by 12 each tick without decrementing it |
| Self-heal | An automatic corrective action that fires without stopping the simulation |
| Deadlock | The state where two or more processes have `p.waiting == true` simultaneously |
| Circular wait | The specific deadlock pattern where P1 waits for P2 and P2 waits for P1 |
| Preemption | Forcing a blocked process to release its claim so the deadlock cycle breaks |
| Starvation | The condition where `p.waitTime >= 3`, meaning a process has been skipped three or more times in a row |
| Priority aging | The corrective action of incrementing `p.priority` when starvation is detected |
| Observation | A plain-English string added to `obs` during a tick, printed in the Holmes section of the dashboard |
| History deque | A sliding window of the last 15 tick values, used for the CPU timeline and memory graph |

---

*Deadlock Holmes — Developer Documentation*  
*CSE323 Operating Systems — North South University — Summer 2025*
