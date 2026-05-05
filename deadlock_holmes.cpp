/*
 * ============================================================
 *       DEADLOCK HOLMES - Self-Healing OS Simulation
 *              Clean ASCII Console Edition
 * ============================================================
 *
 * Compile (MinGW / MSVC on Windows):
 *   g++ -o deadlock_holmes deadlock_holmes.cpp -std=c++17
 *
 * The simulation runs automatically in three acts:
 *   Act 1 - Memory Leak Detection and Self-Healing
 *   Act 2 - Deadlock Detection and Resolution
 *   Act 3 - CPU Starvation and Priority Aging
 *
 * No user input required. Press ENTER at act breaks only.
 * ============================================================
 */

#include <iostream>
#include <vector>
#include <algorithm>
#include <deque>
#include <fstream>
#include <string>
#include <iomanip>
#include <windows.h>

using namespace std;

// ============================================================
//  CONSOLE UTILITIES
// ============================================================

HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

void setColor(int c) { SetConsoleTextAttribute(hConsole, c); }
void resetColor()    { setColor(7); }
void clearScreen()   { system("cls"); }
void wait(int ms)    { Sleep(ms); }

void pressEnterToContinue() {
    setColor(11);
    cout << "\n  Press ENTER to continue...";
    resetColor();
    cin.ignore();
    cin.get();
}

// ============================================================
//  ASCII DRAWING HELPERS
// ============================================================

void drawLine(int width, char ch = '-') {
    cout << "  +" << string(width, ch) << "+\n";
}

void drawBox(const string& title, const vector<string>& lines, int titleColor) {
    const int W = 62;
    setColor(titleColor);
    cout << "\n";
    drawLine(W);

    int pad = (W - (int)title.size()) / 2;
    cout << "  |" << string(pad, ' ') << title
         << string(W - pad - (int)title.size(), ' ') << "|\n";
    drawLine(W);
    resetColor();

    for (const string& l : lines) {
        cout << "  | " << left << setw(W - 1) << l << "|\n";
    }

    setColor(titleColor);
    drawLine(W);
    resetColor();
}

// Build a plain ASCII bar like  [######..........] 42MB
string makeBar(int value, int maxVal, int barWidth, const string& unit = "") {
    int filled = (maxVal > 0) ? (value * barWidth) / maxVal : 0;
    filled = max(0, min(filled, barWidth));
    string s = "[";
    s += string(filled, '#');
    s += string(barWidth - filled, '.');
    s += "] " + to_string(value);
    if (!unit.empty()) s += unit;
    return s;
}

// ============================================================
//  PROCESS STRUCTURE
// ============================================================

struct Process {
    int    id;
    int    memory;
    int    memoryMax;
    int    priority;
    int    waitTime;
    int    waitingFor;
    bool   memoryLeak;
    bool   active;
    bool   waiting;
    string status;
};

deque<int> cpuHistory;
deque<int> memHistory;

// ============================================================
//  DASHBOARD
// ============================================================

void renderHeader(const string& actName, int tick) {
    setColor(11);
    cout << "\n  ============================================================\n";
    cout << "        DEADLOCK HOLMES  -  Self-Healing OS Simulation\n";
    cout << "  ============================================================\n";
    resetColor();
    cout << "  Act  : " << actName << "\n";
    cout << "  Tick : " << tick    << "\n";
    cout << "  ------------------------------------------------------------\n";
}

void renderProcessTable(const vector<Process>& P) {
    cout << "\n  PROCESS TABLE\n";
    cout << "  " << string(60, '-') << "\n";
    cout << "  PID   Memory (100 MB max)              Priority   Status\n";
    cout << "  " << string(60, '-') << "\n";

    for (const auto& p : P) {
        cout << "  P" << p.id << "    ";

        string bar = makeBar(p.memory, 100, 20, "MB");
        while ((int)bar.size() < 30) bar += " ";

        if      (!p.active)         setColor(8);
        else if (p.memory > 70)     setColor(12);
        else if (p.memory > 40)     setColor(14);
        else                        setColor(10);
        cout << bar;
        resetColor();

        cout << "  Pr=" << left << setw(3) << p.priority;

        if      (!p.active)                  setColor(8);
        else if (p.waiting)                  setColor(12);
        else if (p.memoryLeak)               setColor(14);
        else if (p.status == "RUNNING")      setColor(10);
        else if (p.status == "STARVING")     setColor(13);
        else                                 resetColor();

        cout << p.status << "\n";
        resetColor();
    }
    cout << "  " << string(60, '-') << "\n";
}

void renderCpuTimeline() {
    cout << "\n  CPU Timeline  (each block = one tick, oldest on left)\n  ";
    for (int id : cpuHistory) {
        if (id == -1) cout << "[IDLE]";
        else          cout << "[ P" << id << " ]";
    }
    cout << "\n";
}

void renderMemoryGraph() {
    cout << "\n  Total System Memory History  (0=low  9=high)\n  ";
    for (int m : memHistory) {
        int h = min(m / 25, 9);
        if      (m > 150) setColor(12);
        else if (m > 75)  setColor(14);
        else              setColor(10);
        cout << "[" << h << "]";
    }
    resetColor();
    cout << "\n";
}

void renderObservations(const vector<string>& obs) {
    if (obs.empty()) return;
    cout << "\n  HOLMES OBSERVES:\n";
    cout << "  " << string(58, '.') << "\n";
    for (const string& line : obs) {
        setColor(14);
        cout << "  >> ";
        resetColor();
        cout << line << "\n";
    }
    cout << "  " << string(58, '.') << "\n";
}

void renderDashboard(const vector<Process>& P,
                     const string& actName,
                     int tick,
                     const vector<string>& obs) {
    clearScreen();
    renderHeader(actName, tick);
    renderProcessTable(P);
    renderCpuTimeline();
    renderMemoryGraph();
    renderObservations(obs);
}

// ============================================================
//  SCHEDULING  (priority-first with starvation aging)
// ============================================================

int schedule(vector<Process>& P, vector<string>& obs) {
    stable_sort(P.begin(), P.end(),
        [](const Process& a, const Process& b) {
            return a.priority > b.priority;
        });

    int  running = -1;
    bool cpuUsed = false;

    for (auto& p : P) {
        if (!p.active || p.waiting) {
            p.status = p.active ? "BLOCKED" : "DEAD";
            continue;
        }

        if (!cpuUsed) {
            running    = p.id;
            p.status   = "RUNNING";
            p.waitTime = 0;
            cpuUsed    = true;
        } else {
            p.waitTime++;
            p.status = "READY";

            if (p.waitTime >= 3) {
                p.status = "STARVING";
                obs.push_back(
                    "P" + to_string(p.id) + " waited " +
                    to_string(p.waitTime) + " ticks without CPU time. Starvation detected.");
                obs.push_back(
                    "Priority aging: P" + to_string(p.id) +
                    " priority raised from " + to_string(p.priority) +
                    " to " + to_string(p.priority + 1) + ".");
                p.priority++;
                p.waitTime = 0;
            }
        }
    }
    return running;
}

// ============================================================
//  ONE SIMULATION TICK
// ============================================================

void tick(vector<Process>& P,
          int tickNo,
          const string& actName,
          vector<string>& obs) {

    // Memory leak growth
    for (auto& p : P) {
        if (!p.active || p.waiting || !p.memoryLeak) continue;
        int before = p.memory;
        p.memory  += 12;
        obs.push_back(
            "P" + to_string(p.id) + " did not free memory this tick. " +
            to_string(before) + " MB -> " + to_string(p.memory) + " MB.");

        if (p.memory >= p.memoryMax) {
            obs.push_back(
                "DANGER: P" + to_string(p.id) + " reached " +
                to_string(p.memory) + " MB, exceeding the " +
                to_string(p.memoryMax) + " MB limit.");
            obs.push_back(
                "Self-healing: Forcing memory reclaim on P" +
                to_string(p.id) + ". Resetting to 10 MB.");
            p.memory     = 10;
            p.memoryLeak = false;
            obs.push_back(
                "P" + to_string(p.id) +
                " is healthy again. Memory leak cleared.");
        }
    }

    // Deadlock detection
    int deadCount = 0;
    for (auto& p : P) if (p.active && p.waiting) deadCount++;

    if (deadCount >= 2) {
        obs.push_back(
            to_string(deadCount) +
            " processes are waiting on each other. Deadlock confirmed.");

        Process* victim = nullptr;
        for (auto& p : P) {
            if (!p.active || !p.waiting) continue;
            if (!victim || p.priority < victim->priority) victim = &p;
        }
        if (victim) {
            obs.push_back(
                "Preempting P" + to_string(victim->id) +
                " (lowest priority blocked process) to break circular wait.");
            victim->waiting    = false;
            victim->waitingFor = -1;
        }
        for (auto& p : P) {
            if (!p.active || !p.waiting) continue;
            p.waiting    = false;
            p.waitingFor = -1;
            obs.push_back(
                "P" + to_string(p.id) + " unblocked and placed back in ready queue.");
        }
    }

    // Scheduling
    int running = schedule(P, obs);
    if (running == -1)
        obs.push_back("No runnable process found. CPU is idle this tick.");

    // History
    cpuHistory.push_back(running);
    if (cpuHistory.size() > 15) cpuHistory.pop_front();

    int totalMem = 0;
    for (auto& p : P) if (p.active) totalMem += p.memory;
    memHistory.push_back(totalMem);
    if (memHistory.size() > 15) memHistory.pop_front();

    renderDashboard(P, actName, tickNo, obs);
    obs.clear();
}

// ============================================================
//  ACT 1 - MEMORY LEAK
// ============================================================

void actMemoryLeak() {
    clearScreen();
    drawBox("ACT 1 OF 3  -  MEMORY LEAK DETECTION AND SELF-HEALING",
        {
            "",
            "  What is a memory leak?",
            "  A process requests RAM from the OS but never releases it.",
            "  Each tick it holds more and more memory without giving any back.",
            "  Left unchecked, this fills all available RAM and crashes the system.",
            "",
            "  What you will see in this act:",
            "  - Ticks 1-2  : All three processes run normally.",
            "  - Tick 3     : A hidden leak is injected into P2.",
            "  - Ticks 3+   : P2 memory bar grows 12 MB each tick.",
            "  - Threshold  : When P2 exceeds 90 MB, Holmes detects it.",
            "  - Self-heal  : Memory is forcibly reclaimed. P2 resets to 10 MB.",
            ""
        }, 10);

    pressEnterToContinue();

    vector<Process> P = {
        {1, 10, 90, 3, 0, -1, false, true, false, "READY"},
        {2, 15, 90, 2, 0, -1, false, true, false, "READY"},
        {3,  5, 90, 1, 0, -1, false, true, false, "READY"},
    };
    vector<string> obs;
    int t = 1;

    for (int i = 0; i < 2; i++, t++) {
        obs.push_back("All processes running normally. Memory levels stable.");
        tick(P, t, "Act 1 - Memory Leak", obs);
        wait(1600);
    }

    P[1].memoryLeak = true;
    obs.push_back("Memory leak injected into P2. It will accumulate 12 MB per tick.");

    for (int i = 0; i < 12; i++, t++) {
        tick(P, t, "Act 1 - Memory Leak", obs);
        wait(1300);
        if (!P[1].memoryLeak && t > 3) break;
    }

    for (int i = 0; i < 2; i++, t++) {
        obs.push_back("System is stable after self-healing. P2 operating normally.");
        tick(P, t, "Act 1 - Memory Leak", obs);
        wait(1300);
    }

    drawBox("ACT 1 COMPLETE  -  WHAT JUST HAPPENED",
        {
            "",
            "  P2 failed to release memory after each operation.",
            "  Its RAM usage grew from 15 MB by 12 MB every tick.",
            "  Holmes compared each process against a 90 MB danger threshold.",
            "  When P2 crossed that limit, the self-healing routine fired.",
            "  All leaked memory was reclaimed and the process resumed safely.",
            "",
            "  This is similar to how OS watchdog timers and memory guards",
            "  work in production environments to contain runaway processes.",
            ""
        }, 10);

    pressEnterToContinue();
}

// ============================================================
//  ACT 2 - DEADLOCK
// ============================================================

void actDeadlock() {
    clearScreen();
    drawBox("ACT 2 OF 3  -  DEADLOCK DETECTION AND RESOLUTION",
        {
            "",
            "  What is a deadlock?",
            "  P1 holds Resource A and is waiting for Resource B.",
            "  P2 holds Resource B and is waiting for Resource A.",
            "  Neither can ever proceed. Both are stuck permanently.",
            "  This is called a circular wait.",
            "",
            "  What you will see in this act:",
            "  - Ticks 1-2  : Normal operation, no blocking.",
            "  - Tick 3     : P1 and P2 enter a circular wait.",
            "  - Dashboard  : Both show BLOCKED status.",
            "  - Detection  : Holmes counts 2+ mutually blocked processes.",
            "  - Resolution : Lowest-priority process is preempted.",
            "  - Recovery   : All blocked processes are released.",
            ""
        }, 12);

    pressEnterToContinue();

    vector<Process> P = {
        {1, 20, 90, 3, 0, -1, false, true, false, "READY"},
        {2, 20, 90, 3, 0, -1, false, true, false, "READY"},
        {3, 10, 90, 1, 0, -1, false, true, false, "READY"},
    };
    vector<string> obs;
    int t = 1;

    for (int i = 0; i < 2; i++, t++) {
        obs.push_back("All processes running. No resource contention.");
        tick(P, t, "Act 2 - Deadlock", obs);
        wait(1600);
    }

    P[0].waiting = true; P[0].waitingFor = 2;
    P[1].waiting = true; P[1].waitingFor = 1;
    obs.push_back("P1 now holds Resource A and waits for Resource B (held by P2).");
    obs.push_back("P2 now holds Resource B and waits for Resource A (held by P1).");
    obs.push_back("Circular wait formed. Both processes are now deadlocked.");

    for (int i = 0; i < 5; i++, t++) {
        tick(P, t, "Act 2 - Deadlock", obs);
        wait(1300);
        if (!P[0].waiting && !P[1].waiting) break;
    }

    for (int i = 0; i < 2; i++, t++) {
        obs.push_back("Deadlock cleared. All processes progressing normally.");
        tick(P, t, "Act 2 - Deadlock", obs);
        wait(1300);
    }

    drawBox("ACT 2 COMPLETE  -  WHAT JUST HAPPENED",
        {
            "",
            "  P1 and P2 each needed a resource the other was holding.",
            "  Both entered the BLOCKED state. CPU went to P3 instead.",
            "  Holmes detected the circular wait by counting blocked processes.",
            "  The process with the lowest priority was preempted first.",
            "  This broke the cycle. All other blocked processes were freed.",
            "",
            "  This matches the resource preemption strategy for deadlock",
            "  recovery as described in operating systems textbooks.",
            ""
        }, 12);

    pressEnterToContinue();
}

// ============================================================
//  ACT 3 - STARVATION
// ============================================================

void actStarvation() {
    clearScreen();
    drawBox("ACT 3 OF 3  -  CPU STARVATION AND PRIORITY AGING",
        {
            "",
            "  What is CPU starvation?",
            "  Priority scheduling always gives the CPU to the highest",
            "  priority process. A very low-priority process may never get",
            "  scheduled if higher-priority ones keep arriving.",
            "",
            "  What you will see in this act:",
            "  - P1 starts at priority 5, P2 at priority 4, P3 at priority 1.",
            "  - P1 runs every tick. P3 is always at the back of the queue.",
            "  - After 3 consecutive waits, Holmes flags P3 as starving.",
            "  - Priority aging gives P3 +1 priority each time it starves.",
            "  - P3 priority climbs until it can finally win the CPU.",
            ""
        }, 13);

    pressEnterToContinue();

    vector<Process> P = {
        {1, 15, 90, 5, 0, -1, false, true, false, "READY"},
        {2, 15, 90, 4, 0, -1, false, true, false, "READY"},
        {3,  5, 90, 1, 0, -1, false, true, false, "READY"},
    };
    vector<string> obs;
    int t = 1;

    obs.push_back("P1=priority 5, P2=priority 4, P3=priority 1.");
    obs.push_back("Scheduler gives CPU to highest priority first. P3 will be skipped.");

    for (int i = 0; i < 14; i++, t++) {
        tick(P, t, "Act 3 - CPU Starvation", obs);
        wait(1100);
    }

    drawBox("ACT 3 COMPLETE  -  WHAT JUST HAPPENED",
        {
            "",
            "  P3 was skipped every tick because P1 and P2 had higher priority.",
            "  After every 3 skipped ticks, Holmes detected starvation.",
            "  Priority aging gave P3 one extra priority point each time.",
            "  Eventually P3 priority caught up and it received CPU time.",
            "",
            "  Priority aging is used in real schedulers including those in",
            "  Linux and Windows NT to prevent indefinite process starvation.",
            ""
        }, 13);

    pressEnterToContinue();
}

// ============================================================
//  CSV EXPORT
// ============================================================

void exportCSV() {
    ofstream cpu("cpu_history.csv");
    cpu << "Tick,ProcessID\n";
    int c = 1;
    for (int id : cpuHistory) cpu << c++ << "," << id << "\n";

    ofstream mem("memory_history.csv");
    mem << "Tick,TotalMemoryMB\n";
    c = 1;
    for (int m : memHistory) mem << c++ << "," << m << "\n";
}

// ============================================================
//  SPLASH SCREEN
// ============================================================

void splashScreen() {
    clearScreen();
    setColor(11);
    cout << "\n\n";
    cout << "  ============================================================\n";
    cout << "                     DEADLOCK  HOLMES\n";
    cout << "          A Self-Healing Operating System Simulation\n";
    cout << "  ============================================================\n\n";
    resetColor();

    cout << "  This program simulates three classic operating system\n";
    cout << "  failure modes and demonstrates how a self-healing system\n";
    cout << "  detects and corrects each problem automatically.\n\n";

    cout << "  The simulation advances on its own. You only need to press\n";
    cout << "  ENTER between acts.\n\n";

    setColor(14);
    cout << "  Act 1 - Memory Leak\n";
    cout << "          A process leaks RAM every tick until Holmes heals it.\n\n";
    cout << "  Act 2 - Deadlock\n";
    cout << "          Two processes block each other. Holmes breaks the cycle.\n\n";
    cout << "  Act 3 - CPU Starvation\n";
    cout << "          A low-priority process is skipped until aging saves it.\n\n";
    resetColor();

    cout << "  How to read the dashboard:\n\n";
    cout << "  [######......] 42MB   Memory bar. More # means more RAM in use.\n";
    cout << "  [P1][P2][IDLE]        CPU Timeline. Who ran each tick.\n";
    cout << "  [0][2][5][8]          Memory load history. 0=low, 9=high.\n";
    cout << "  >> text               Holmes explaining what just happened.\n\n";

    cout << "  Status meanings:\n";
    setColor(10);  cout << "  RUNNING  "; resetColor(); cout << " The process has the CPU this tick.\n";
    setColor(14);  cout << "  LEAKING  "; resetColor(); cout << " Memory is growing abnormally.\n";
    setColor(13);  cout << "  STARVING "; resetColor(); cout << " Process has been waiting too long.\n";
    setColor(12);  cout << "  BLOCKED  "; resetColor(); cout << " Process is stuck in a deadlock.\n";
    setColor(8);   cout << "  DEAD     "; resetColor(); cout << " Process has been terminated.\n";
    cout << "\n";

    pressEnterToContinue();
}

// ============================================================
//  MAIN
// ============================================================

int main() {
    SetConsoleOutputCP(437);

    splashScreen();

    actMemoryLeak();
    actDeadlock();
    actStarvation();

    clearScreen();
    setColor(11);
    cout << "\n  ============================================================\n";
    cout << "                    SIMULATION COMPLETE\n";
    cout << "  ============================================================\n\n";
    resetColor();

    cout << "  Act 1  Memory leak in P2 grew by 12 MB each tick.\n";
    cout << "         Self-healing reset memory to 10 MB when 90 MB was reached.\n\n";

    cout << "  Act 2  P1 and P2 formed a circular wait, freezing both.\n";
    cout << "         Preempting the lowest-priority process resolved the deadlock.\n\n";

    cout << "  Act 3  P3 was starved by higher-priority P1 and P2.\n";
    cout << "         Priority aging boosted P3 until it could finally run.\n\n";

    exportCSV();
    setColor(8);
    cout << "  Logs saved: cpu_history.csv  and  memory_history.csv\n\n";
    resetColor();

    cout << "  Press ENTER to exit.";
    cin.get();
    return 0;
}
