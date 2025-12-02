#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <cstdint>
#include <string>

// Frame structure
struct Frame {
    int frame_id;
    int process_id = -1;
    int page_number = -1;
    bool is_free = true;
    uint64_t last_access_time = 0;

    // [FIX]: The Frame now acts as Physical RAM.
    // It holds the actual data, not just metadata.
    std::vector<uint16_t> data;
};

// Page Table Entry
struct PageTableEntry {
    int frame_number = -1;
    bool valid = false;
    uint64_t last_access_time = 0;
};

// Memory statistics
struct MemoryStats {
    int total_frames;
    int used_frames;
    int free_frames;
    int total_page_faults = 0;
    int total_pages_in = 0;
    int total_pages_out = 0;
};

class MemoryManager {
public:
    static MemoryManager& getInstance();

    void initialize(int max_overall_mem, int mem_per_frame);
    bool allocateMemory(int process_id, int process_memory_size);
    void deallocateMemory(int process_id);

    // Memory access methods
    bool readMemory(int process_id, int virtual_address, uint16_t& value);
    bool writeMemory(int process_id, int virtual_address, uint16_t value);

    // Page fault handling
    bool handlePageFault(int process_id, int page_number);

    // Statistics
    MemoryStats getStats();
    void printMemorySnapshot();

    // Helper methods
    int getFrameForProcess(int process_id, int page_number);
    bool isInitialized() const { return initialized; }

private:
    MemoryManager() = default;
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    int findFreeFrame();
    int evictPage(); // LRU page replacement
    uint64_t getCurrentTime();

    // Backing Store Helpers
    void saveFrameToBackingStore(int frame_id, int process_id, int page_num);
    bool loadFrameFromBackingStore(int frame_id, int process_id, int page_num);

    bool initialized = false;
    int max_overall_mem = 0;
    int mem_per_frame = 0;
    int total_frames = 0;

    std::vector<Frame> frames;
    std::unordered_map<int, std::vector<PageTableEntry>> page_tables; // process_id -> page table
    std::unordered_map<int, int> process_memory_sizes; // process_id -> total memory size

    // [FIX]: Simulation of Disk Drive (Backing Store)
    // Map Key: "PID_PageNum", Value: Vector of data
    // We update the text file whenever this changes.
    std::unordered_map<std::string, std::vector<uint16_t>> backing_store_disk;

    MemoryStats stats;
    std::mutex memory_mutex;
    uint64_t access_counter = 0;
};

#endif // MEMORY_MANAGER_H
