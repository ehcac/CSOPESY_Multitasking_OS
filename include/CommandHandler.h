#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <thread>
#include <mutex>
#include <string>
#include <queue>
#include <atomic>

class CommandHandler {
public:
    static CommandHandler& getInstance();
    
    void queueCommand(const std::string& command);
    bool hasCommand();
    std::string getNextCommand();
    void processCommand(const std::string& command, int& next_pid);
    
    void setRunning(bool running) { is_running = running; }
    bool isRunning() const { return is_running; }
    void setInitialized(bool init) { initialized = init; }
    bool isInitialized() const { return initialized; }

private:
    CommandHandler() = default;
    CommandHandler(const CommandHandler&) = delete;
    CommandHandler& operator=(const CommandHandler&) = delete;
    
    std::queue<std::string> command_queue;
    std::mutex command_queue_mutex;
    std::atomic<bool> is_running{true};
    std::atomic<bool> initialized{false};
};

#endif // COMMAND_HANDLER_H