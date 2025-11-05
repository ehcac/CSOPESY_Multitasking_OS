#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <queue>
#include <unordered_map>
#include <fstream>
#include <random>
#include <algorithm>
#include <ctime>

// --- Shared State and Thread Control ---
std::atomic<bool> is_running{ true };
std::atomic<bool> initialized{ false };
std::atomic<bool> scheduler_running{ false };
std::vector<std::thread> cpu_threads;

std::random_device rd;
std::mt19937 rng(rd());

// Config variables from config.txt
int num_cpu;
std::string scheduler;
int quantum_cycles;
int batch_process_freq;
int max_ins;
int min_ins;
int delays_per_exec;

// Command queue
std::queue<std::string> command_queue;
std::mutex command_queue_mutex;

// CPU utilization tracking
std::vector<bool> cpu_busy;
std::vector<int> cpu_process_count;
std::mutex cpu_stats_mutex;

// ----- Process Instruction Definition -----
enum InstructionType {
    PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR_LOOP
};

struct Instruction {
    InstructionType type;
    std::string msg;
    std::string var;
    std::string var1, var2, var3;
    uint16_t value2 = 0, value3 = 0;
    bool isVar2 = true, isVar3 = true;
    uint8_t sleepTicks = 0;
    std::vector<Instruction> nestedInstructions;
    int repeatCount = 0;
};

// Process Control Block
struct PCB {
    int pid;
    std::string name;
    int pc = 0;
    std::unordered_map<std::string, uint16_t> vars;
    std::vector<Instruction> instructions;
    int sleep_ticks = 0;
    bool finished = false;
    std::vector<std::string> screenBuffer;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    int cpu_core = -1;
    int total_instructions = 0;
    std::mutex pcb_mutex;
};

// --- Process Management ---
std::unordered_map<std::string, PCB*> all_processes;
std::unordered_map<int, PCB*> pid_to_process;
std::mutex process_map_mutex;

std::queue<PCB*> readyQueue;
std::mutex readyQueueMutex;

std::thread process_generator_thread;

// Screen state
enum ScreenMode { MAIN_MENU, PROCESS_SCREEN };
ScreenMode current_screen = MAIN_MENU;
std::string current_process_name = "";
std::mutex screen_mutex;

// --- Utility Functions ---
void clear_screen() {
    std::cout << "\033[2J\033[1;1H";
}

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::tm timeinfo;
#ifdef _WIN32
    localtime_s(&timeinfo, &time);
#else
    localtime_r(&time, &timeinfo);
#endif

    std::stringstream ss;
    ss << std::put_time(&timeinfo, "%m/%d/%Y %I:%M:%S%p");
    return ss.str();
}

// --- Instruction Generation ---
Instruction make_random_instruction(int depth) {
    Instruction inst;
    int type;

    if (depth >= 3) {
        type = rng() % 5;
    }
    else {
        type = rng() % 6;
    }

    switch (type) {
    case 0: // PRINT
        inst.type = PRINT;
        inst.var = "";
        break;

    case 1: // DECLARE
        inst.type = DECLARE;
        inst.var1 = "x" + std::to_string(rng() % 5);
        inst.value2 = rng() % 500;
        break;

    case 2: // ADD
        inst.type = ADD;
        inst.var1 = "x" + std::to_string(rng() % 5);
        inst.isVar2 = (rng() % 2) == 0;
        inst.isVar3 = (rng() % 2) == 0;
        if (inst.isVar2) {
            inst.var2 = "x" + std::to_string(rng() % 5);
        }
        else {
            inst.value2 = rng() % 500;
        }
        if (inst.isVar3) {
            inst.var3 = "x" + std::to_string(rng() % 5);
        }
        else {
            inst.value3 = rng() % 500;
        }
        break;

    case 3: // SUBTRACT
        inst.type = SUBTRACT;
        inst.var1 = "x" + std::to_string(rng() % 5);
        inst.isVar2 = (rng() % 2) == 0;
        inst.isVar3 = (rng() % 2) == 0;
        if (inst.isVar2) {
            inst.var2 = "x" + std::to_string(rng() % 5);
        }
        else {
            inst.value2 = rng() % 500;
        }
        if (inst.isVar3) {
            inst.var3 = "x" + std::to_string(rng() % 5);
        }
        else {
            inst.value3 = rng() % 500;
        }
        break;

    case 4: // SLEEP
        inst.type = SLEEP;
        inst.sleepTicks = (rng() % 5) + 1;
        break;

    case 5: // FOR_LOOP
        inst.type = FOR_LOOP;
        inst.repeatCount = (rng() % 3) + 2;
        int nested_count = (rng() % 3) + 1;
        for (int i = 0; i < nested_count; i++) {
            inst.nestedInstructions.push_back(make_random_instruction(depth + 1));
        }
        break;
    }

    return inst;
}

PCB* create_random_process(int pid) {
    PCB* p = new PCB();
    p->pid = pid;
    p->name = "process_" + std::to_string(pid);
    p->start_time = std::chrono::system_clock::now();

    int num_instructions = min_ins + (rng() % (max_ins - min_ins + 1));
    p->total_instructions = num_instructions;

    for (int i = 0; i < num_instructions; i++) {
        p->instructions.push_back(make_random_instruction(0));
    }

    return p;
}

PCB* create_named_process(const std::string& name, int pid) {
    PCB* p = create_random_process(pid);
    p->name = name;
    return p;
}

// --- Instruction Execution ---
void execute_instruction(PCB& p, Instruction& inst) {
    switch (inst.type) {
    case PRINT: {
        std::string output;
        if (!inst.var.empty()) {
            output = "Hello world from " + p.name + "! Value: " + std::to_string(p.vars[inst.var]);
        }
        else {
            output = "Hello world from " + p.name + "!";
        }
        p.screenBuffer.push_back("(" + get_timestamp() + ") " + output);
        break;
    }
    case DECLARE:
        p.vars[inst.var1] = inst.value2;
        break;

    case ADD: {
        uint16_t a = inst.isVar2 ? p.vars[inst.var2] : inst.value2;
        uint16_t b = inst.isVar3 ? p.vars[inst.var3] : inst.value3;
        p.vars[inst.var1] = a + b;
        break;
    }

    case SUBTRACT: {
        uint16_t a = inst.isVar2 ? p.vars[inst.var2] : inst.value2;
        uint16_t b = inst.isVar3 ? p.vars[inst.var3] : inst.value3;
        p.vars[inst.var1] = a - b;
        break;
    }

    case SLEEP:
        p.sleep_ticks = inst.sleepTicks;
        return;

    case FOR_LOOP: {
        for (int i = 0; i < inst.repeatCount; i++) {
            for (auto& nested : inst.nestedInstructions) {
                execute_instruction(p, nested);
                if (p.finished) return;
            }
        }
        break;
    }
    }

    p.pc++;
    if (p.pc >= (int)p.instructions.size()) {
        p.finished = true;
        p.end_time = std::chrono::system_clock::now();
    }
}

// --- CPU Worker ---
void cpu_worker(int id) {
    PCB* current_process = nullptr;
    int current_run_cycles = 0; // tracks cycles for the current quantum

    while (scheduler_running) {
        // --- 1. Load a New Process if CPU is Idle ---
        if (current_process == nullptr) {
            std::unique_lock<std::mutex> lock(readyQueueMutex);
            if (!readyQueue.empty()) {
                current_process = readyQueue.front();
                readyQueue.pop();
                current_run_cycles = 0;
            }
        }

        // --- 2. Execute or Handle Current Process ---
        if (current_process) {
            bool process_finished_this_run = false;
            bool process_preempted_this_run = false;

            // --- Update CPU Busy status (Lock A) ---
            {
                std::lock_guard<std::mutex> lock(cpu_stats_mutex);
                cpu_busy[id] = true;
            }

            // --- Do all process work (Lock B) ---
            {
                std::lock_guard<std::mutex> pcb_lock(current_process->pcb_mutex);

                current_process->cpu_core = id; // Set core ID (protected by pcb_mutex)

                // Handle Sleep (I/O)
                if (current_process->sleep_ticks > 0) {
                    current_process->sleep_ticks--;
                    if (current_process->sleep_ticks == 0) {
                        current_process->pc++; // Woke up, increment PC
                        if (current_process->pc >= (int)current_process->instructions.size()) {
                            current_process->finished = true;
                            current_process->end_time = std::chrono::system_clock::now();
                        }
                    }
                    process_preempted_this_run = true; // Yield CPU
                }
                // Execute the instruction
                else {
                    execute_instruction(*current_process, current_process->instructions[current_process->pc]);
                    current_run_cycles++;

                    // Check for Termination
                    if (current_process->finished) {
                        process_finished_this_run = true;
                    }
                    // Preemption
                    else if (current_run_cycles >= quantum_cycles) {
                        process_preempted_this_run = true;
                    }
                }
            } // --- pcb_mutex (Lock B) is RELEASED here ---


            // --- Post-cycle cleanup (NO pcb_mutex held) ---

            if (process_finished_this_run) {
                // Now it's safe to lock global stats
                {
                    std::lock_guard<std::mutex> lock(cpu_stats_mutex);
                    cpu_process_count[id]++;
                }
                current_process = nullptr; // Drop the process
            }
            else if (process_preempted_this_run) {
                // Sleeping or preempted, re-queue it
                if (!current_process->finished) {
                    std::lock_guard<std::mutex> lock(readyQueueMutex);
                    readyQueue.push(current_process);
                }
                current_process = nullptr; // Give up the process
            }
        }

        // --- Update busy status (if idle) ---
        if (current_process == nullptr) {
            std::lock_guard<std::mutex> lock(cpu_stats_mutex);
            cpu_busy[id] = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(delays_per_exec));
    }
}

// --- Screen Commands ---
void display_process_screen(const std::string& process_name) {
    std::lock_guard<std::mutex> lock(process_map_mutex);

    if (all_processes.find(process_name) == all_processes.end()) {
        std::cout << "Process '" << process_name << "' not found.\n";
        return;
    }

    PCB* p = all_processes[process_name];
    std::lock_guard<std::mutex> pcb_lock(p->pcb_mutex);
    clear_screen();

    std::cout << "Process: " << p->name << "\n";
    std::cout << "ID: " << p->pid << "\n";

    if (p->finished) {
        std::cout << "\nFinished!\n\n";
    }
    else {
        std::cout << "Current instruction line: " << p->pc << "\n";
        std::cout << "Lines of code: " << p->total_instructions << "\n";
    }

    std::cout << "\n--- Logs ---\n";
    int start = std::max(0, (int)p->screenBuffer.size() - 20);
    for (int i = start; i < (int)p->screenBuffer.size(); i++) {
        std::cout << p->screenBuffer[i] << "\n";
    }

    if (p->finished) {
        std::cout << "\nFinished!\n";
    }
    std::cout << "\n";
}

void process_smi() {
    if (current_process_name.empty()) {
        std::cout << "Not in a process screen.\n";
        return;
    }
    display_process_screen(current_process_name);
}

void screen_ls() {
    // --- 1. Calculate Global Stats (Requires Locks) ---
    int cores_used = 0;
    {
        std::lock_guard<std::mutex> clock(cpu_stats_mutex);
        for (int i = 0; i < num_cpu; i++) {
            if (cpu_busy[i]) cores_used++;
        }
    }

    int running = 0, finished = 0;
    std::vector<PCB*> process_list_copy;
    {
        // Copy the process pointers to iterate outside of the global lock
        std::lock_guard<std::mutex> plock(process_map_mutex);
        for (const auto& pair : all_processes) {
            process_list_copy.push_back(pair.second);
            if (pair.second->finished) finished++;
            else running++;
        }
    }

    // --- 2. Display Stats ---
    std::cout << "\nCPU Utilization: " << (cores_used * 100 / num_cpu) << "%\n";
    std::cout << "Cores used: " << cores_used << "\n";
    std::cout << "Cores available: " << (num_cpu - cores_used) << "\n";
    std::cout << "\nRunning processes: " << running << "\n";
    std::cout << "Finished processes: " << finished << "\n";
    std::cout << "+---------------+--------------------------+----------+-----------------------------------+" << std::endl;

    // --- 3. Display Table (Acquire PCB lock one at a time) ---
    for (PCB* p : process_list_copy) {
        std::lock_guard<std::mutex> pcb_lock(p->pcb_mutex);

        auto time = std::chrono::system_clock::to_time_t(p->start_time);
        std::tm timeinfo;
#ifdef _WIN32
        localtime_s(&timeinfo, &time);
#else
        localtime_r(&time, &timeinfo);
#endif

        std::stringstream ss;
        ss << std::put_time(&timeinfo, "%m/%d/%Y %I:%M:%S%p");
        std::cout << "| " << std::left << std::setw(14) << p->name << "| ";
        std::cout << " (" << ss.str() << ") " << "| ";
        if (p->finished) {
            std::cout << std::right << std::setw(7) << "Done" << "| ";
        }
        else {
            std::cout << std::right << std::setw(7) << "Core: " << p->cpu_core << " | ";
            //std::cout << p->cpu_core << "    ";
        }
        int barWidth = 20;
        int filled = (p->pc * barWidth) / p->total_instructions;

        std::cout << "[";
        for (int i = 0; i < barWidth; i++) {
            if (i < filled) std::cout << "=";
            else std::cout << " ";
        }
        std::cout << "] " << std::right << std::setw(3) << p->pc << " / " << p->total_instructions << " |\n";
    }
    std::cout << "+---------------+--------------------------+----------+-----------------------------------+" << std::endl;
}

void report_util() {
    std::ofstream report("csopesy-log.txt");
    if (!report.is_open()) {
        std::cout << "Error: Could not create report file.\n";
        return;
    }

    std::lock_guard<std::mutex> plock(process_map_mutex);
    std::lock_guard<std::mutex> clock(cpu_stats_mutex);

    int cores_used = 0;
    for (int i = 0; i < num_cpu; i++) {
        if (cpu_busy[i]) cores_used++;
    }

    int running = 0, finished = 0;
    std::vector<PCB*> running_processes, finished_processes;

    for (const auto& pair : all_processes) {
        if (pair.second->finished) {
            finished++;
            finished_processes.push_back(pair.second);
        }
        else {
            running++;
            running_processes.push_back(pair.second);
        }
    }

    report << "CPU Utilization Report\n";
    report << "Generated: " << get_timestamp() << "\n\n";
    report << "CPU Utilization: " << (static_cast<double>(cores_used) * 100.0 / num_cpu) << "%\n";
    report << "Cores used: " << cores_used << "\n";
    report << "Cores available: " << (num_cpu - cores_used) << "\n";
    report << "Running processes: " << running << "\n";
    report << "Finished processes: " << finished << "\n\n";

    report << "--------------------------------------\n";

    report << "Running processes:\n";
    for (PCB* p : running_processes) {
        std::lock_guard<std::mutex> pcb_lock(p->pcb_mutex);
        auto time = std::chrono::system_clock::to_time_t(p->start_time);
        std::tm timeinfo;
#ifdef _WIN32
        localtime_s(&timeinfo, &time);
#else
        localtime_r(&time, &timeinfo);
#endif

        std::stringstream ss;
        ss << std::put_time(&timeinfo, "%m/%d/%Y %I:%M:%S%p");

        report << p->name << "    (" << ss.str() << ")    Core: "
            << p->cpu_core << "    " << p->pc << " / " << p->total_instructions << "\n";
    }

    report << "\nFinished processes:\n";
    for (PCB* p : finished_processes) {
        std::lock_guard<std::mutex> pcb_lock(p->pcb_mutex);
        auto time = std::chrono::system_clock::to_time_t(p->start_time);
        std::tm timeinfo;
#ifdef _WIN32
        localtime_s(&timeinfo, &time);
#else
        localtime_r(&time, &timeinfo);
#endif

        std::stringstream ss;
        ss << std::put_time(&timeinfo, "%m/%d/%Y %I:%M:%S%p");

        report << p->name << "    (" << ss.str() << ")    Finished    "
            << p->pc << " / " << p->total_instructions << "\n";
    }

    report << "--------------------------------------\n";

    report.close();
    std::cout << "Report generated: csopesy-log.txt\n";
}

// --- Keyboard Handler ---
void keyboard_handler_thread_func() {
    std::string command_line;
    while (is_running) {
        if (std::getline(std::cin, command_line)) {
            if (!command_line.empty()) {

                std::unique_lock<std::mutex> lock(command_queue_mutex);
                command_queue.push(command_line);
                lock.unlock();
            }
        }
        else {
            is_running = false;
        }
    }
}

// --- Main Function ---
int main() {
    clear_screen();
    std::cout << "CSOPESY CPU Scheduler Simulator\n\n";
    std::cout << "Group Developers:\n";
    std::cout << "1. Matthew Copon\n";
    std::cout << "2. Chastine Cabatay\n";
    std::cout << "3. Ericson Tan\n";
    std::cout << "4. Joaquin Cardino\n";
    std::cout << "Version: 1.00.00\n\n";

    std::cout << "Command >> " << std::flush;

    std::thread keyboard_handler_thread(keyboard_handler_thread_func);

    int next_pid = 1;

    while (is_running) {
        std::string command_line;
        bool command_processed = false;
        {
            std::unique_lock<std::mutex> lock(command_queue_mutex);
            if (!command_queue.empty()) {
                command_line = command_queue.front();
                command_queue.pop();
                command_processed = true;
            }
        }

        if (!command_line.empty()) {
            std::istringstream iss(command_line);
            std::string cmd;
            iss >> cmd;

            if (cmd == "exit") {
                if (current_screen == PROCESS_SCREEN) {
                    current_screen = MAIN_MENU;
                    current_process_name = "";
                    clear_screen();
                    std::cout << "Returned to main menu.\n\n";
                }
                else {
                    is_running = false;
                }
            }
            else if (cmd == "initialize") {
                std::ifstream configFile("config.txt");
                if (!configFile.is_open()) {
                    std::cerr << "ERROR: config.txt could not be opened.\n";
                }
                else {
                    std::string key, value;
                    while (configFile >> key >> value) {
                        if (key == "num-cpu")
                            num_cpu = std::stoi(value);
                        else if (key == "scheduler")
                            scheduler = value.substr(1, value.size() - 2);
                        else if (key == "quantum-cycles")
                            quantum_cycles = std::stoi(value);
                        else if (key == "batch-processes-freq")
                            batch_process_freq = std::stoi(value);
                        else if (key == "min-ins")
                            min_ins = std::stoi(value);
                        else if (key == "max-ins")
                            max_ins = std::stoi(value);
                        else if (key == "delay-per-exec")
                            delays_per_exec = std::stoi(value);
                    }

                    std::cout << batch_process_freq;

                    cpu_busy.resize(num_cpu);
                    cpu_process_count.resize(num_cpu, 0);

                    initialized = true;
                    std::cout << "Console initialized successfully.\n";
                }
            }
            else if (cmd == "scheduler-start") {
                if (!initialized) {
                    std::cout << "ERROR: Console not initialized.\n";
                }
                else if (scheduler_running) {
                    std::cout << "ERROR: Scheduler already running.\n";
                }
                else {
                    std::cout << "Scheduler started.\n";
                    scheduler_running = true;

                    for (int i = 0; i < num_cpu; i++) {
                        cpu_threads.emplace_back(cpu_worker, i);
                    }

                    process_generator_thread = std::thread([&next_pid]() {
                        while (scheduler_running) {
                            PCB* p = create_random_process(next_pid++);
                            {
                                std::lock_guard<std::mutex> plock(process_map_mutex);
                                all_processes[p->name] = p;
                                pid_to_process[p->pid] = p;
                            }
                            {
                                std::lock_guard<std::mutex> qlock(readyQueueMutex);
                                readyQueue.push(p);
                            }
                            std::this_thread::sleep_for(std::chrono::seconds(batch_process_freq));
                        }
                        });
                }
            }
            else if (cmd == "scheduler-stop") {
                if (!scheduler_running) {
                    std::cout << "ERROR: Scheduler not running.\n";
                }
                else {
                    std::cout << "Stopping scheduler...\n";
                    scheduler_running = false;

                    if (process_generator_thread.joinable())
                        process_generator_thread.join();

                    for (auto& t : cpu_threads) {
                        if (t.joinable()) t.join();
                    }
                    cpu_threads.clear();

                    {
                        std::lock_guard<std::mutex> lock(cpu_stats_mutex);
                        // Set all cores back to false (idle)
                        std::fill(cpu_busy.begin(), cpu_busy.end(), false);
                    }

                    std::cout << "Scheduler stopped.\n";
                }
            }
            else if (cmd == "report-util") {
                if (!initialized) {
                    std::cout << "ERROR: Console not initialized.\n";
                }
                else {
                    report_util();
                }
            }
            else if (cmd == "screen") {
                if (!initialized) {
                    std::cout << "ERROR: Console not initialized.\n";
                    // The prompt will be printed below after the switch
                }
                else {
                    std::string subcmd;
                    iss >> subcmd;

                    if (subcmd == "-s") {
                        std::string proc_name;
                        iss >> proc_name;

                        if (proc_name.empty()) {
                            std::cout << "Usage: screen -s <process_name>\n";
                        }
                        else {
                            PCB* p = create_named_process(proc_name, next_pid++);
                            {
                                std::lock_guard<std::mutex> plock(process_map_mutex);
                                all_processes[p->name] = p;
                                pid_to_process[p->pid] = p;
                            }
                            {
                                std::lock_guard<std::mutex> qlock(readyQueueMutex);
                                readyQueue.push(p);
                            }

                            current_screen = PROCESS_SCREEN;
                            current_process_name = proc_name;
                            display_process_screen(proc_name);
                        }
                    }
                    else if (subcmd == "-r") {
                        std::string proc_name;
                        iss >> proc_name;

                        if (proc_name.empty()) {
                            std::cout << "Usage: screen -r <process_name>\n";
                        }
                        else {
                            bool found = false;
                            {
                                std::lock_guard<std::mutex> lock(process_map_mutex);
                                found = (all_processes.find(proc_name) != all_processes.end());
                            }

                            if (found) {
                                current_screen = PROCESS_SCREEN;
                                current_process_name = proc_name;
                                display_process_screen(proc_name);
                            }
                            else {
                                std::cout << "Process '" << proc_name << "' not found.\n";
                            }
                        }
                    }
                    else if (subcmd == "-ls") {
                        screen_ls();
                    }
                    else {
                        std::cout << "Usage: screen -s <name> | screen -r <name> | screen -ls\n";
                    }
                }
            }
            else if (cmd == "process-smi") {
                if (current_screen == PROCESS_SCREEN) {
                    process_smi();
                }
                else {
                    std::cout << "Not in a process screen. Use 'screen -r <name>' or 'screen -s <name>' first.\n";
                }
            }
            else {
                std::cout << "Command not found.\n";
            }

            if (is_running && command_processed) {
                std::cout << "Command >> " << std::flush;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Cleaning up resources..." << std::endl;

    // Stop the scheduler if it's still running
    if (scheduler_running) {
        scheduler_running = false;
        if (process_generator_thread.joinable())
            process_generator_thread.join();
        for (auto& t : cpu_threads)
            if (t.joinable()) t.join();
    }

    if (keyboard_handler_thread.joinable())
        keyboard_handler_thread.join();

    // Clean up all processes
    {
        std::lock_guard<std::mutex> lock(process_map_mutex);
        for (auto& pair : all_processes) {
            delete pair.second; // Delete the PCB object
        }
        all_processes.clear();
        pid_to_process.clear();
    }

    // Clean up any remaining processes in the ready queue
    {
        std::lock_guard<std::mutex> lock(readyQueueMutex);
        while (!readyQueue.empty()) {
            delete readyQueue.front();
            readyQueue.pop();
        }
    }

    std::cout << "Cleanup complete. Exiting." << std::endl;
    return 0;
}
