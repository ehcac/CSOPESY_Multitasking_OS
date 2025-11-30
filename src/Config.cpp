#include <mutex>
#include <thread>
#include "../include/Config.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

// Config Implementation
Config& Config::getInstance() {
    static Config instance;
    return instance;
}

bool Config::loadFromFile(const std::string& filename) {
    std::ifstream configFile(filename);
    if (!configFile.is_open()) {
        std::cerr << "ERROR: " << filename << " could not be opened.\n";
        return false;
    }
    
    std::string key, value;
    while (configFile >> key >> value) {
        if (key == "num-cpu")
            num_cpu = std::stoi(value);
        else if (key == "scheduler")
            scheduler = value.substr(1, value.size() - 2);
        else if (key == "quantum-cycles")
            quantum_cycles = std::stoi(value);
        else if (key == "batch-processes-freq")
            batch_process_freq = std::stoi(value);
        else if (key == "min-ins")
            min_ins = std::stoi(value);
        else if (key == "max-ins")
            max_ins = std::stoi(value);
        else if (key == "delay-per-exec")
            delays_per_exec = std::stoi(value);
    }
    
    return true;
}

// Utils Implementation
namespace Utils {
    void clearScreen() {
        std::cout << "\033[2J\033[1;1H";
    }

    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::tm timeinfo;
#ifdef _WIN32
        localtime_s(&timeinfo, &time);
#else
        localtime_r(&time, &timeinfo);
#endif

        std::stringstream ss;
        ss << std::put_time(&timeinfo, "%m/%d/%Y %I:%M:%S%p");
        return ss.str();
    }
}