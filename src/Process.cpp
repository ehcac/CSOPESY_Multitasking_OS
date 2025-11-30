#include <mutex>
#include "../include/Process.h"
#include "../include/Config.h"
#include <chrono>
#include <thread>


// ============ ProcessGenerator Implementation ============
ProcessGenerator::ProcessGenerator() : rng(std::random_device{}()) {}

Instruction ProcessGenerator::makeRandomInstruction(int depth) {
    Instruction inst;
    int type = (depth >= 3) ? (rng() % 5) : (rng() % 6);

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
        } else {
            inst.value2 = rng() % 500;
        }
        if (inst.isVar3) {
            inst.var3 = "x" + std::to_string(rng() % 5);
        } else {
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
        } else {
            inst.value2 = rng() % 500;
        }
        if (inst.isVar3) {
            inst.var3 = "x" + std::to_string(rng() % 5);
        } else {
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
            inst.nestedInstructions.push_back(makeRandomInstruction(depth + 1));
        }
        break;
    }

    return inst;
}

PCB* ProcessGenerator::createRandomProcess(int pid) {
    Config& config = Config::getInstance();
    
    PCB* p = new PCB();
    p->pid = pid;
    p->name = "process_" + std::to_string(pid);
    p->start_time = std::chrono::system_clock::now();

    int num_instructions = config.getMinIns() + 
                          (rng() % (config.getMaxIns() - config.getMinIns() + 1));
    p->total_instructions = num_instructions;

    for (int i = 0; i < num_instructions; i++) {
        p->instructions.push_back(makeRandomInstruction(0));
    }

    return p;
}

PCB* ProcessGenerator::createNamedProcess(const std::string& name, int pid) {
    PCB* p = createRandomProcess(pid);
    p->name = name;
    return p;
}

// ============ InstructionExecutor Implementation ============
void InstructionExecutor::execute(PCB& p, Instruction& inst) {
    switch (inst.type) {
    case PRINT: {
        std::string output;
        if (!inst.var.empty()) {
            output = "Hello world from " + p.name + "! Value: " + 
                    std::to_string(p.vars[inst.var]);
        } else {
            output = "Hello world from " + p.name + "!";
        }
        p.screenBuffer.push_back("(" + Utils::getTimestamp() + ") " + output);
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
                execute(p, nested);
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

// ============ ProcessManager Implementation ============
ProcessManager& ProcessManager::getInstance() {
    static ProcessManager instance;
    return instance;
}

void ProcessManager::addProcess(PCB* process) {
    std::lock_guard<std::mutex> lock(process_map_mutex);
    all_processes[process->name] = process;
    pid_to_process[process->pid] = process;
}

PCB* ProcessManager::getProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(process_map_mutex);
    auto it = all_processes.find(name);
    return (it != all_processes.end()) ? it->second : nullptr;
}

PCB* ProcessManager::getProcess(int pid) {
    std::lock_guard<std::mutex> lock(process_map_mutex);
    auto it = pid_to_process.find(pid);
    return (it != pid_to_process.end()) ? it->second : nullptr;
}

bool ProcessManager::processExists(const std::string& name) {
    std::lock_guard<std::mutex> lock(process_map_mutex);
    return all_processes.find(name) != all_processes.end();
}

std::unordered_map<std::string, PCB*> ProcessManager::getAllProcesses() {
    std::lock_guard<std::mutex> lock(process_map_mutex);
    return all_processes;
}

void ProcessManager::cleanup() {
    std::lock_guard<std::mutex> lock(process_map_mutex);
    for (auto& pair : all_processes) {
        delete pair.second;
    }
    all_processes.clear();
    pid_to_process.clear();
}