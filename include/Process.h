#ifndef PROCESS_H
#define PROCESS_H

#include <thread>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <cstdint>
#include <random>
#include <mutex>

// Instruction Types
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

// Process Generator
class ProcessGenerator {
public:
    ProcessGenerator();
    Instruction makeRandomInstruction(int depth = 0);
    PCB* createRandomProcess(int pid);
    PCB* createNamedProcess(const std::string& name, int pid);

private:
    std::mt19937 rng;
};

// Instruction Executor
class InstructionExecutor {
public:
    static void execute(PCB& process, Instruction& instruction);
};

// Process Manager
class ProcessManager {
public:
    static ProcessManager& getInstance();
    
    void addProcess(PCB* process);
    PCB* getProcess(const std::string& name);
    PCB* getProcess(int pid);
    bool processExists(const std::string& name);
    std::unordered_map<std::string, PCB*> getAllProcesses();
    void cleanup();
    std::mutex& getProcessMapMutex() { return process_map_mutex; }

private:
    ProcessManager() = default;
    ProcessManager(const ProcessManager&) = delete;
    ProcessManager& operator=(const ProcessManager&) = delete;
    
    std::unordered_map<std::string, PCB*> all_processes;
    std::unordered_map<int, PCB*> pid_to_process;
    std::mutex process_map_mutex;
};

#endif // PROCESS_H