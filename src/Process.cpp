#include <mutex>
#include "../include/Process.h"
#include "../include/Config.h"
#include "../include/MemoryManager.h"
#include <chrono>
#include <thread>
#include <sstream>
#include <algorithm>
#include <iostream>


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

Instruction ProcessGenerator::parseInstruction(const std::string& inst_str) {
    Instruction inst;
    std::string trimmed = inst_str;
    
    // Trim whitespace
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    std::istringstream iss(trimmed);
    std::string cmd;
    iss >> cmd;
    
    // Convert to uppercase for comparison
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    
    if (cmd == "PRINT") {
        inst.type = PRINT;
        inst.var = "";
    }
    else if (cmd == "DECLARE") {
        inst.type = DECLARE;
        iss >> inst.var1 >> inst.value2;
    }
    else if (cmd == "ADD") {
        inst.type = ADD;
        std::string operand2, operand3;
        iss >> inst.var1 >> operand2 >> operand3;
        
        // Check if operand2 is a variable or value
        if (operand2[0] == 'x' || operand2[0] == 'X') {
            inst.isVar2 = true;
            inst.var2 = operand2;
        } else {
            inst.isVar2 = false;
            inst.value2 = std::stoi(operand2);
        }
        
        // Check if operand3 is a variable or value
        if (operand3[0] == 'x' || operand3[0] == 'X') {
            inst.isVar3 = true;
            inst.var3 = operand3;
        } else {
            inst.isVar3 = false;
            inst.value3 = std::stoi(operand3);
        }
    }
    else if (cmd == "SUBTRACT") {
        inst.type = SUBTRACT;
        std::string operand2, operand3;
        iss >> inst.var1 >> operand2 >> operand3;
        
        if (operand2[0] == 'x' || operand2[0] == 'X') {
            inst.isVar2 = true;
            inst.var2 = operand2;
        } else {
            inst.isVar2 = false;
            inst.value2 = std::stoi(operand2);
        }
        
        if (operand3[0] == 'x' || operand3[0] == 'X') {
            inst.isVar3 = true;
            inst.var3 = operand3;
        } else {
            inst.isVar3 = false;
            inst.value3 = std::stoi(operand3);
        }
    }
    else if (cmd == "SLEEP") {
        inst.type = SLEEP;
        int ticks;
        iss >> ticks;
        inst.sleepTicks = ticks;
    }
    else if (cmd == "READ") {
        inst.type = READ;
        iss >> inst.var >> inst.memory_address;
    }
    else if (cmd == "WRITE") {
        inst.type = WRITE;
        iss >> inst.memory_address >> inst.write_value;
    }
    else {
        throw std::runtime_error("invalid command");
    }
    
    return inst;
}

PCB* ProcessGenerator::createRandomProcess(int pid, int memory_size) {
    Config& config = Config::getInstance();
    
    PCB* p = new PCB();
    p->pid = pid;
    p->name = "process_" + std::to_string(pid);
    p->start_time = std::chrono::system_clock::now();
    p->memory_size = memory_size;

    int num_instructions = config.getMinIns() + 
                          (rng() % (config.getMaxIns() - config.getMinIns() + 1));
    p->total_instructions = num_instructions;

    for (int i = 0; i < num_instructions; i++) {
        p->instructions.push_back(makeRandomInstruction(0));
    }

    return p;
}

PCB* ProcessGenerator::createNamedProcess(const std::string& name, int pid, int memory_size) {
    PCB* p = createRandomProcess(pid, memory_size);
    p->name = name;
    return p;
}

PCB* ProcessGenerator::createCustomProcess(const std::string& name, int pid, int memory_size, const std::string& instructions_str) {
    PCB* p = new PCB();
    p->pid = pid;
    p->name = name;
    p->start_time = std::chrono::system_clock::now();
    p->memory_size = memory_size;
    
    // Parse instructions string
    std::istringstream iss(instructions_str);
    std::string inst_str;
    std::vector<std::string> inst_list;
    
    // Split by semicolon
    while (std::getline(iss, inst_str, ';')) {
        if (!inst_str.empty()) {
            inst_list.push_back(inst_str);
        }
    }
    
    // Validate instruction count
    if (inst_list.size() < 1 || inst_list.size() > 50) {
        throw std::runtime_error("invalid command");
    }
    
    // Parse each instruction
    for (const auto& inst : inst_list) {
        try {
            p->instructions.push_back(parseInstruction(inst));
        } catch (const std::exception& e) {
            delete p;
            throw;
        }
    }
    
    p->total_instructions = p->instructions.size();
    
    return p;
}

// ============ InstructionExecutor Implementation ============
void InstructionExecutor::execute(PCB& p, Instruction& inst) {
    MemoryManager& mm = MemoryManager::getInstance();
    
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
    
    case READ: {
        uint16_t value = 0;
        if (mm.readMemory(p.pid, inst.memory_address, value)) {
            p.vars[inst.var] = value;
            p.screenBuffer.push_back("(" + Utils::getTimestamp() + ") READ " + 
                                    inst.var + " from address " + 
                                    std::to_string(inst.memory_address) + 
                                    " = " + std::to_string(value));
        } else {
            p.screenBuffer.push_back("(" + Utils::getTimestamp() + ") ERROR: Failed to read from address " + 
                                    std::to_string(inst.memory_address));
        }
        break;
    }
    
    case WRITE: {
        if (mm.writeMemory(p.pid, inst.memory_address, inst.write_value)) {
            p.screenBuffer.push_back("(" + Utils::getTimestamp() + ") WRITE " + 
                                    std::to_string(inst.write_value) + 
                                    " to address " + 
                                    std::to_string(inst.memory_address));
        } else {
            p.screenBuffer.push_back("(" + Utils::getTimestamp() + ") ERROR: Failed to write to address " + 
                                    std::to_string(inst.memory_address));
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
    MemoryManager& mm = MemoryManager::getInstance();
    std::lock_guard<std::mutex> lock(process_map_mutex);
    
    for (auto& pair : all_processes) {
        mm.deallocateMemory(pair.second->pid);
        delete pair.second;
    }
    all_processes.clear();
    pid_to_process.clear();
}