#ifndef CONFIG_H
#define CONFIG_H

#include <mutex>
#include <string>
#include <thread>


// Configuration Management
class Config {
public:
    static Config& getInstance();
    bool loadFromFile(const std::string& filename);

    int getNumCPU() const { return num_cpu; }
    std::string getScheduler() const { return scheduler; }
    int getQuantumCycles() const { return quantum_cycles; }
    int getBatchProcessFreq() const { return batch_process_freq; }
    int getMaxIns() const { return max_ins; }
    int getMinIns() const { return min_ins; }
    int getDelaysPerExec() const { return delays_per_exec; }
    int getMaxOverAll() const { return max_overall_mem; }
    int getMemPerFrame() const { return mem_per_frame; }
    int getMinMemPerProc() const { return mim_mem_per_proc; }
    int getMaxMemPerProc() const { return max_mem_per_prc; }

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    int num_cpu = 0;
    std::string scheduler;
    int quantum_cycles = 0;
    int batch_process_freq = 0;
    int max_ins = 0;
    int min_ins = 0;
    int delays_per_exec = 0;
    int max_overall_mem = 0;
    int mem_per_frame = 0;
    int mim_mem_per_proc = 0;
    int max_mem_per_prc = 0;
};

// Utility Functions
namespace Utils {
    void clearScreen();
    std::string getTimestamp();
}

#endif // CONFIG_H