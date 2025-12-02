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
            
            std::cout << "\n=== Virtual Memory Statistics ===\n";
            std::cout << "Total Memory: " << config.getMaxOverAll() << " bytes\n";
            std::cout << "Frame Size: " << config.getMemPerFrame() << " bytes\n";
            std::cout << "Total Frames: " << stats.total_frames << "\n";
            std::cout << "Used Frames: " << stats.used_frames << "\n";
            std::cout << "Free Frames: " << stats.free_frames << "\n";
            std::cout << "Memory Utilization: " 
                      << (stats.total_frames > 0 ? 
                          (stats.used_frames * 100.0 / stats.total_frames) : 0) 
                      << "%\n";
            std::cout << "\n--- Page Fault Statistics ---\n";
            std::cout << "Total Page Faults: " << stats.total_page_faults << "\n";
            std::cout << "Pages Loaded (In): " << stats.total_pages_in << "\n";
            std::cout << "Pages Evicted (Out): " << stats.total_pages_out << "\n";
            std::cout << "\n";
            
            // Optional: Print detailed frame allocation
            mm.printMemorySnapshot();
        }
    }
    else if (cmd == "process-smi") {
        screen.processSMI();
    }
    else {
        std::cout << "Command not found.\n";
    }
}