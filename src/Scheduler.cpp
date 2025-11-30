#include <mutex>
#include "../include/Scheduler.h"
#include "../include/Config.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <fstream>


// ============ Scheduler Implementation ============
Scheduler& Scheduler::getInstance() {
    static Scheduler instance;
    return instance;
}

bool Scheduler::start() {
    if (scheduler_running) {
        std::cout << "ERROR: Scheduler already running.\n";
        return false;
    }

    Config& config = Config::getInstance();
    int num_cpu = config.getNumCPU();

    cpu_busy.resize(num_cpu, false);
    cpu_process_count.resize(num_cpu, 0);

    scheduler_running = true;

    for (int i = 0; i < num_cpu; i++) {
        cpu_threads.emplace_back(&Scheduler::cpuWorker, this, i);
    }

    process_generator_thread = std::thread(&Scheduler::processGeneratorWorker, this);

    std::cout << "Scheduler started.\n";
    return true;
}

bool Scheduler::stop() {
    if (!scheduler_running) {
        std::cout << "ERROR: Scheduler not running.\n";
        return false;
    }

    std::cout << "Stopping scheduler...\n";
    scheduler_running = false;

    if (process_generator_thread.joinable()) {
        process_generator_thread.join();
    }

    for (auto& t : cpu_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    cpu_threads.clear();

    {
        std::lock_guard<std::mutex> lock(cpu_stats_mutex);
        std::fill(cpu_busy.begin(), cpu_busy.end(), false);
    }

    std::cout << "Scheduler stopped.\n";
    return true;
}

void Scheduler::enqueueProcess(PCB* process) {
    std::lock_guard<std::mutex> lock(ready_queue_mutex);
    ready_queue.push(process);
}

int Scheduler::getCoresUsed() {
    std::lock_guard<std::mutex> lock(cpu_stats_mutex);
    int count = 0;
    for (bool busy : cpu_busy) {
        if (busy) count++;
    }
    return count;
}

std::vector<bool> Scheduler::getCPUBusy() {
    std::lock_guard<std::mutex> lock(cpu_stats_mutex);
    return cpu_busy;
}

void Scheduler::cpuWorker(int id) {
    Config& config = Config::getInstance();
    PCB* current_process = nullptr;
    int current_run_cycles = 0;

    while (scheduler_running) {
        if (current_process == nullptr) {
            std::unique_lock<std::mutex> lock(ready_queue_mutex);
            if (!ready_queue.empty()) {
                current_process = ready_queue.front();
                ready_queue.pop();
                current_run_cycles = 0;
            }
        }

        if (current_process) {
            bool process_finished_this_run = false;
            bool process_preempted_this_run = false;

            {
                std::lock_guard<std::mutex> lock(cpu_stats_mutex);
                cpu_busy[id] = true;
            }

            {
                std::lock_guard<std::mutex> pcb_lock(current_process->pcb_mutex);
                current_process->cpu_core = id;

                if (current_process->sleep_ticks > 0) {
                    current_process->sleep_ticks--;
                    if (current_process->sleep_ticks == 0) {
                        current_process->pc++;
                        if (current_process->pc >= (int)current_process->instructions.size()) {
                            current_process->finished = true;
                            current_process->end_time = std::chrono::system_clock::now();
                        }
                    }
                    process_preempted_this_run = true;
                }
                else {
                    InstructionExecutor::execute(*current_process,
                        current_process->instructions[current_process->pc]);
                    current_run_cycles++;

                    if (current_process->finished) {
                        process_finished_this_run = true;
                    }
                    else if (current_run_cycles >= config.getQuantumCycles()) {
                        process_preempted_this_run = true;
                    }
                }
            }

            if (process_finished_this_run) {
                {
                    std::lock_guard<std::mutex> lock(cpu_stats_mutex);
                    cpu_process_count[id]++;
                }
                current_process = nullptr;
            }
            else if (process_preempted_this_run) {
                if (!current_process->finished) {
                    std::lock_guard<std::mutex> lock(ready_queue_mutex);
                    ready_queue.push(current_process);
                }
                current_process = nullptr;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(config.getDelaysPerExec()));
    }
}

void Scheduler::processGeneratorWorker() {
    Config& config = Config::getInstance();
    ProcessManager& pm = ProcessManager::getInstance();

    while (scheduler_running) {
        PCB* p = process_generator.createRandomProcess(next_pid++);
        pm.addProcess(p);
        enqueueProcess(p);

        std::this_thread::sleep_for(std::chrono::seconds(config.getBatchProcessFreq()));
    }
}

// ============ ScreenManager Implementation ============
ScreenManager& ScreenManager::getInstance() {
    static ScreenManager instance;
    return instance;
}

void ScreenManager::displayProcessScreen(const std::string& process_name) {
    ProcessManager& pm = ProcessManager::getInstance();
    PCB* p = pm.getProcess(process_name);

    if (!p) {
        std::cout << "Process '" << process_name << "' not found.\n";
        return;
    }

    std::lock_guard<std::mutex> pcb_lock(p->pcb_mutex);
    Utils::clearScreen();

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

void ScreenManager::processSMI() {
    if (current_process_name.empty()) {
        std::cout << "Not in a process screen.\n";
        return;
    }
    displayProcessScreen(current_process_name);
}

void ScreenManager::screenLS() {
    Config& config = Config::getInstance();
    Scheduler& scheduler = Scheduler::getInstance();
    ProcessManager& pm = ProcessManager::getInstance();

    int cores_used = scheduler.getCoresUsed();
    int num_cpu = config.getNumCPU();

    int running = 0, finished = 0;
    std::vector<PCB*> process_list_copy;

    auto all_processes = pm.getAllProcesses();
    for (const auto& pair : all_processes) {
        process_list_copy.push_back(pair.second);
        if (pair.second->finished) finished++;
        else running++;
    }

    std::cout << "\nCPU Utilization: " << (cores_used * 100 / num_cpu) << "%\n";
    std::cout << "Cores used: " << cores_used << "\n";
    std::cout << "Cores available: " << (num_cpu - cores_used) << "\n";
    std::cout << "\nRunning processes: " << running << "\n";
    std::cout << "Finished processes: " << finished << "\n";
    std::cout << "+---------------+--------------------------+----------+-----------------------------------+" << std::endl;

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
        }

        int barWidth = 20;
        int filled = (p->pc * barWidth) / p->total_instructions;

        std::cout << "[";
        for (int i = 0; i < barWidth; i++) {
            if (i < filled) std::cout << "=";
            else std::cout << " ";
        }
        std::cout << "] " << std::right << std::setw(3) << p->pc << " / "
            << p->total_instructions << " |\n";
    }
    std::cout << "+---------------+--------------------------+----------+-----------------------------------+" << std::endl;
}

void ScreenManager::reportUtil() {
    Config& config = Config::getInstance();
    Scheduler& scheduler = Scheduler::getInstance();
    ProcessManager& pm = ProcessManager::getInstance();

    std::ofstream report("csopesy-log.txt");
    if (!report.is_open()) {
        std::cout << "Error: Could not create report file.\n";
        return;
    }

    int cores_used = scheduler.getCoresUsed();
    int num_cpu = config.getNumCPU();

    int running = 0, finished = 0;
    std::vector<PCB*> running_processes, finished_processes;

    auto all_processes = pm.getAllProcesses();
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
    report << "Generated: " << Utils::getTimestamp() << "\n\n";
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