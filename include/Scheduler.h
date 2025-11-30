#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "Process.h"
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <string>

// CPU Scheduler
class Scheduler {
public:
    static Scheduler& getInstance();
    
    bool start();
    bool stop();
    bool isRunning() const { return scheduler_running; }
    void enqueueProcess(PCB* process);
    int getCoresUsed();
    std::vector<bool> getCPUBusy();
    std::mutex& getReadyQueueMutex() { return ready_queue_mutex; }

private:
    Scheduler() = default;
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    
    void cpuWorker(int id);
    void processGeneratorWorker();
    
    std::atomic<bool> scheduler_running{false};
    std::vector<std::thread> cpu_threads;
    std::thread process_generator_thread;
    
    std::queue<PCB*> ready_queue;
    std::mutex ready_queue_mutex;
    
    std::vector<bool> cpu_busy;
    std::vector<int> cpu_process_count;
    std::mutex cpu_stats_mutex;
    
    ProcessGenerator process_generator;
    int next_pid = 1;
};

// Screen Mode
enum ScreenMode { MAIN_MENU, PROCESS_SCREEN };

// Screen Manager
class ScreenManager {
public:
    static ScreenManager& getInstance();
    
    void displayProcessScreen(const std::string& process_name);
    void processSMI();
    void screenLS();
    void reportUtil();
    
    void setCurrentScreen(ScreenMode mode) { current_screen = mode; }
    ScreenMode getCurrentScreen() const { return current_screen; }
    void setCurrentProcessName(const std::string& name) { current_process_name = name; }
    std::string getCurrentProcessName() const { return current_process_name; }

private:
    ScreenManager() = default;
    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;
    
    ScreenMode current_screen = MAIN_MENU;
    std::string current_process_name = "";
    std::mutex screen_mutex;
};

#endif // SCHEDULER_H