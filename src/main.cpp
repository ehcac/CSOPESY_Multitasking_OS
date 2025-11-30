#include "../include/Config.h"
#include "../include/Scheduler.h"
#include "../include/Process.h"
#include "../include/CommandHandler.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>


void keyboardHandlerThread() {
    CommandHandler& handler = CommandHandler::getInstance();
    std::string command_line;
    
    while (handler.isRunning()) {
        if (std::getline(std::cin, command_line)) {
            if (!command_line.empty()) {
                handler.queueCommand(command_line);
            }
        } else {
            handler.setRunning(false);
        }
    }
}

int main() {
    Utils::clearScreen();
    std::cout << "CSOPESY CPU Scheduler Simulator\n\n";
    std::cout << "Group Developers:\n";
    std::cout << "1. Matthew Copon\n";
    std::cout << "2. Chastine Cabatay\n";
    std::cout << "3. Ericson Tan\n";
    std::cout << "4. Joaquin Cardino\n";
    std::cout << "Version: 1.00.00\n\n";
    std::cout << "Command >> " << std::flush;

    CommandHandler& handler = CommandHandler::getInstance();
    Scheduler& scheduler = Scheduler::getInstance();
    ProcessManager& pm = ProcessManager::getInstance();
    
    std::thread keyboard_thread(keyboardHandlerThread);
    
    int next_pid = 1;

    while (handler.isRunning()) {
        if (handler.hasCommand()) {
            std::string command = handler.getNextCommand();
            if (!command.empty()) {
                handler.processCommand(command, next_pid);
                
                if (handler.isRunning()) {
                    std::cout << "Command >> " << std::flush;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Cleaning up resources..." << std::endl;

    if (scheduler.isRunning()) {
        scheduler.stop();
    }

    if (keyboard_thread.joinable()) {
        keyboard_thread.join();
    }

    pm.cleanup();

    std::cout << "Cleanup complete. Exiting." << std::endl;
    return 0;
}