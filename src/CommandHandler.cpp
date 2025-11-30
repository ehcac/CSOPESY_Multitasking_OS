#include <thread>
#include <mutex>
#include "../include/CommandHandler.h"
#include "../include/Config.h"
#include "../include/Scheduler.h"
#include "../include/Process.h"
#include <iostream>
#include <sstream>

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

void CommandHandler::processCommand(const std::string& command_line, int& next_pid) {
    std::istringstream iss(command_line);
    std::string cmd;
    iss >> cmd;

    ScreenManager& screen = ScreenManager::getInstance();
    Scheduler& scheduler = Scheduler::getInstance();
    ProcessManager& pm = ProcessManager::getInstance();
    Config& config = Config::getInstance();

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
                iss >> proc_name;

                if (proc_name.empty()) {
                    std::cout << "Usage: screen -s <process_name>\n";
                }
                else {
                    ProcessGenerator gen;
                    PCB* p = gen.createNamedProcess(proc_name, next_pid++);
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
                    // TODO: check if works
                    // MO2 UPDATE: screen -r also prints an error message when process is not finished
                }
                else {
                    // We get the pointer to the process to check its status
                    PCB* p = pm.getProcess(proc_name);

                    // Check if process exists AND if it has FINISHED.
                    if (p != nullptr && p->finished) {
                        screen.setCurrentScreen(PROCESS_SCREEN);
                        screen.setCurrentProcessName(proc_name);
                        screen.displayProcessScreen(proc_name);
                    }
                    else {
                        // This block executes if:
                        // 1. Process is nullptr (doesn't exist)
                        // 2. OR Process exists but p->finished is false (still running)
                        std::cout << "Process '" << proc_name << "' not found.\n";
                    }
                }
            }
            else if (subcmd == "-ls") {
                screen.screenLS();
            }
            else {
                std::cout << "Usage: screen -s <n> | screen -r <n> | screen -ls\n";
            }
        }
    }
    // UPDATE: command can now be accessed in main menu
    else if (cmd == "process-smi") {
        screen.processSMI();
    }
    // TODO: add vmstat
    else {
        std::cout << "Command not found.\n";
    }
}