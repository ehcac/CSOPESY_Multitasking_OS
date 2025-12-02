#include <thread>
#include <mutex>
#include "../include/CommandHandler.h"
#include "../include/Config.h"
#include "../include/Scheduler.h"
#include "../include/Process.h"
#include "../include/MemoryManager.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <iomanip>

CommandHandler& CommandHandler::getInstance() {
    static CommandHandler instance;
    return instance;
}

void CommandHandler::queueCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(command_queue_mutex);
    command_queue.push(command);
}

bool CommandHandler::hasCommand() {
    std::lock_guard<std::mutex> lock(command_queue_mutex);
    return !command_queue.empty();
}

std::string CommandHandler::getNextCommand() {
    std::lock_guard<std::mutex> lock(command_queue_mutex);
    if (command_queue.empty()) return "";

    std::string cmd = command_queue.front();
    command_queue.pop();
    return cmd;
}

bool isPowerOfTwo(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

void CommandHandler::processCommand(const std::string& command_line, int& next_pid) {
    std::istringstream iss(command_line);
    std::string cmd;
    iss >> cmd;

    ScreenManager& screen = ScreenManager::getInstance();
    Scheduler& scheduler = Scheduler::getInstance();
    ProcessManager& pm = ProcessManager::getInstance();
    Config& config = Config::getInstance();
    MemoryManager& mm = MemoryManager::getInstance();

    if (cmd == "exit") {
        if (screen.getCurrentScreen() == PROCESS_SCREEN) {
            screen.setCurrentScreen(MAIN_MENU);
            screen.setCurrentProcessName("");
            Utils::clearScreen();
            std::cout << "Returned to main menu.\n\n";
        }
        else {
            is_running = false;
        }
    }
    else if (cmd == "initialize") {
        if (config.loadFromFile("config.txt")) {
            initialized = true;
            
            // Initialize memory manager if memory settings are provided
            int max_mem = config.getMaxOverAll();
            int mem_per_frame = config.getMemPerFrame();
            
            if (max_mem > 0 && mem_per_frame > 0) {
                mm.initialize(max_mem, mem_per_frame);
            }
            
            std::cout << "Console initialized successfully.\n";
        }
    }
    else if (cmd == "scheduler-start") {
        if (!initialized) {
            std::cout << "ERROR: Console not initialized.\n";
        }
        else {
            scheduler.start();
        }
    }
    else if (cmd == "scheduler-stop") {
        scheduler.stop();
    }
    else if (cmd == "report-util") {
        if (!initialized) {
            std::cout << "ERROR: Console not initialized.\n";
        }
        else {
            screen.reportUtil();
        }
    }
    else if (cmd == "screen") {
        if (!initialized) {
            std::cout << "ERROR: Console not initialized.\n";
        }
        else {
            std::string subcmd;
            iss >> subcmd;

            if (subcmd == "-s") {
                std::string proc_name;
                int memory_size = 0;
                iss >> proc_name;
                
                // Check if memory size is provided
                if (iss >> memory_size) {
                    // Validate memory size
                    if (!isPowerOfTwo(memory_size)) {
                        std::cout << "ERROR: Process memory size must be a power of 2.\n";
                        return;
                    }
                    
                    if (memory_size < 64 || memory_size > 65536) {
                        std::cout << "ERROR: Process memory size must be between 64 and 65536.\n";
                        return;
                    }
                }

                if (proc_name.empty()) {
                    std::cout << "Usage: screen -s <process_name> [<memory_size>]\n";
                }
                else {
                    ProcessGenerator gen;
                    PCB* p = gen.createNamedProcess(proc_name, next_pid++, memory_size);
                    
                    // Allocate memory if memory size specified and memory manager initialized
                    if (memory_size > 0 && mm.isInitialized()) {
                        if (!mm.allocateMemory(p->pid, memory_size)) {
                            std::cout << "ERROR: Failed to allocate memory for process.\n";
                            delete p;
                            return;
                        }
                    }
                    
                    pm.addProcess(p);
                    scheduler.enqueueProcess(p);

                    screen.setCurrentScreen(PROCESS_SCREEN);
                    screen.setCurrentProcessName(proc_name);
                    screen.displayProcessScreen(proc_name);
                }
            }
            else if (subcmd == "-r") {
                std::string proc_name;
                iss >> proc_name;

                if (proc_name.empty()) {
                    std::cout << "Usage: screen -r <process_name>\n";
                }
                else {
                    PCB* p = pm.getProcess(proc_name);

                    if (p != nullptr && p->finished) {
                        screen.setCurrentScreen(PROCESS_SCREEN);
                        screen.setCurrentProcessName(proc_name);
                        screen.displayProcessScreen(proc_name);
                    }
                    else {
                        std::cout << "Process '" << proc_name << "' not found.\n";
                    }
                }
            }
            else if (subcmd == "-ls") {
                screen.screenLS();
            }
            else if (subcmd == "-c") {
                std::string proc_name;
                int memory_size;
                iss >> proc_name >> memory_size;
                
                // Validate memory size
                if (!isPowerOfTwo(memory_size)) {
                    std::cout << "ERROR: Process memory size must be a power of 2.\n";
                    return;
                }
                
                if (memory_size < 64 || memory_size > 65536) {
                    std::cout << "ERROR: Process memory size must be between 64 and 65536.\n";
                    return;
                }
                
                // Get the instructions string - everything after memory_size
                std::string instructions_str;
                std::getline(iss, instructions_str);
                
                // Trim leading whitespace
                instructions_str.erase(0, instructions_str.find_first_not_of(" \t\n\r"));
                
                // Remove quotes if present
                if (!instructions_str.empty() && instructions_str.front() == '"') {
                    instructions_str.erase(0, 1);
                }
                if (!instructions_str.empty() && instructions_str.back() == '"') {
                    instructions_str.pop_back();
                }
                
                if (proc_name.empty() || instructions_str.empty()) {
                    std::cout << "Usage: screen -c <process_name> <process_memory_size> \"<instructions>\"\n";
                    return;
                }
                
                try {
                    ProcessGenerator gen;
                    PCB* p = gen.createCustomProcess(proc_name, next_pid++, memory_size, instructions_str);
                    
                    // Allocate memory if memory manager initialized
                    if (mm.isInitialized()) {
                        if (!mm.allocateMemory(p->pid, memory_size)) {
                            std::cout << "ERROR: Failed to allocate memory for process.\n";
                            delete p;
                            return;
                        }
                    }
                    
                    pm.addProcess(p);
                    scheduler.enqueueProcess(p);
                    
                    screen.setCurrentScreen(PROCESS_SCREEN);
                    screen.setCurrentProcessName(proc_name);
                    screen.displayProcessScreen(proc_name);
                }
                catch (const std::exception& e) {
                    std::cout << "ERROR: " << e.what() << "\n";
                }
            }
            else {
                std::cout << "Usage: screen -s <process_name> [<memory_size>] | screen -r <process_name> | screen -ls | screen -c <process_name> <process_memory_size> \"<instructions>\"\n";
            }
        }
    }
    else if (cmd == "vmstat") {
        if (!initialized) {
            std::cout << "ERROR: Console not initialized.\n";
        }
        else if (!mm.isInitialized()) {
            std::cout << "ERROR: Memory Manager not initialized.\n";
        }
        else {
             MemoryStats stats = mm.getStats();
             Config& config = Config::getInstance();
             Scheduler& scheduler = Scheduler::getInstance();
            
             long long total_mem = config.getMaxOverAll();
             long long used_mem = (long long)stats.used_frames * config.getMemPerFrame();
             long long free_mem = (long long)stats.free_frames * config.getMemPerFrame();
            
             // In this simulation, Used Memory is effectively Active Memory
             long long active_mem = used_mem;
             long long inactive_mem = 0; // Not simulated, but listed in standard vmstat
            
             // CPU Ticks
             unsigned long long active_ticks = scheduler.getActiveTicks();
             unsigned long long idle_ticks = scheduler.getIdleTicks();
             unsigned long long system_ticks = 0; 
            
             std::cout << "\n";
             std::cout << std::setw(12) << total_mem << " bytes total memory\n";
             std::cout << std::setw(12) << used_mem << " bytes used memory\n";
             std::cout << std::setw(12) << active_mem << " bytes active memory\n";
             std::cout << std::setw(12) << inactive_mem << " bytes inactive memory\n";
             std::cout << std::setw(12) << free_mem << " bytes free memory\n";
             std::cout << std::setw(12) << total_mem << " bytes total swap\n";
             std::cout << std::setw(12) << total_mem << " bytes free swap\n";
             std::cout << std::setw(12) << active_ticks << " non-nice user cpu ticks\n";
             std::cout << std::setw(12) << idle_ticks << " idle cpu ticks\n";
             std::cout << std::setw(12) << stats.total_pages_in << " pages paged in\n";
             std::cout << std::setw(12) << stats.total_pages_out << " pages paged out\n";
             std::cout << std::setw(12) << Utils::getTimestamp() << " boot time\n";
             std::cout << std::setw(12) << (next_pid - 1) << " forks\n";
             std::cout << "\n";
        }
    }
    else if (cmd == "process-smi") {
        screen.processSMI();
    }
    else {
        std::cout << "Command not found.\n";
    }
}
