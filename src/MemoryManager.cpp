#include "../include/MemoryManager.h"
#include "../include/Config.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <iomanip>

MemoryManager& MemoryManager::getInstance() {
    static MemoryManager instance;
    return instance;
}

void MemoryManager::initialize(int max_mem, int frame_size) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    max_overall_mem = max_mem;
    mem_per_frame = frame_size;
    total_frames = max_overall_mem / mem_per_frame;
    
    frames.clear();
    frames.resize(total_frames);
    
    for (int i = 0; i < total_frames; i++) {
        frames[i].frame_id = i;
        frames[i].is_free = true;
        frames[i].process_id = -1;
        frames[i].page_number = -1;
    }
    
    stats.total_frames = total_frames;
    stats.used_frames = 0;
    stats.free_frames = total_frames;
    stats.total_page_faults = 0;
    stats.total_pages_in = 0;
    stats.total_pages_out = 0;
    
    initialized = true;
    
    std::cout << "Memory Manager initialized: " << total_frames << " frames, " 
              << mem_per_frame << " bytes per frame.\n";
}

bool MemoryManager::allocateMemory(int process_id, int process_memory_size) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    if (!initialized) {
        std::cerr << "ERROR: Memory Manager not initialized.\n";
        return false;
    }
    
    // Check if power of 2
    if ((process_memory_size & (process_memory_size - 1)) != 0) {
        std::cerr << "ERROR: Process memory size must be a power of 2.\n";
        return false;
    }
    
    // Check bounds
    if (process_memory_size < 64 || process_memory_size > 65536) {
        std::cerr << "ERROR: Process memory size must be between 64 and 65536.\n";
        return false;
    }
    
    int num_pages = process_memory_size / mem_per_frame;
    if (num_pages == 0) num_pages = 1;
    
    // Initialize page table for this process (demand paging - no frames allocated yet)
    page_tables[process_id].resize(num_pages);
    for (int i = 0; i < num_pages; i++) {
        page_tables[process_id][i].valid = false;
        page_tables[process_id][i].frame_number = -1;
    }
    
    process_memory_sizes[process_id] = process_memory_size;
    memory_data[process_id] = std::unordered_map<int, uint16_t>();
    
    return true;
}

void MemoryManager::deallocateMemory(int process_id) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    if (page_tables.find(process_id) == page_tables.end()) {
        return;
    }
    
    // Free all frames used by this process
    for (auto& frame : frames) {
        if (frame.process_id == process_id) {
            frame.is_free = true;
            frame.process_id = -1;
            frame.page_number = -1;
            stats.used_frames--;
            stats.free_frames++;
        }
    }
    
    page_tables.erase(process_id);
    process_memory_sizes.erase(process_id);
    memory_data.erase(process_id);
}

int MemoryManager::findFreeFrame() {
    for (int i = 0; i < total_frames; i++) {
        if (frames[i].is_free) {
            return i;
        }
    }
    return -1;
}

int MemoryManager::evictPage() {
    // LRU page replacement
    uint64_t oldest_time = UINT64_MAX;
    int victim_frame = -1;
    
    for (int i = 0; i < total_frames; i++) {
        if (!frames[i].is_free && frames[i].last_access_time < oldest_time) {
            oldest_time = frames[i].last_access_time;
            victim_frame = i;
        }
    }
    
    if (victim_frame != -1) {
        Frame& frame = frames[victim_frame];
        
        // Invalidate page table entry
        if (page_tables.find(frame.process_id) != page_tables.end()) {
            if (frame.page_number < (int)page_tables[frame.process_id].size()) {
                page_tables[frame.process_id][frame.page_number].valid = false;
                page_tables[frame.process_id][frame.page_number].frame_number = -1;
            }
        }
        
        stats.total_pages_out++;
        frame.is_free = true;
        frame.process_id = -1;
        frame.page_number = -1;
    }
    
    return victim_frame;
}

bool MemoryManager::handlePageFault(int process_id, int page_number) {
    if (page_tables.find(process_id) == page_tables.end()) {
        return false;
    }
    
    if (page_number >= (int)page_tables[process_id].size()) {
        return false;
    }
    
    int frame_id = findFreeFrame();
    
    if (frame_id == -1) {
        // No free frames, need to evict
        frame_id = evictPage();
        if (frame_id == -1) {
            return false; // Could not evict
        }
    }
    
    // Allocate frame to page
    frames[frame_id].is_free = false;
    frames[frame_id].process_id = process_id;
    frames[frame_id].page_number = page_number;
    frames[frame_id].last_access_time = getCurrentTime();
    
    page_tables[process_id][page_number].valid = true;
    page_tables[process_id][page_number].frame_number = frame_id;
    page_tables[process_id][page_number].last_access_time = getCurrentTime();
    
    stats.used_frames++;
    stats.free_frames--;
    stats.total_page_faults++;
    stats.total_pages_in++;
    
    return true;
}

bool MemoryManager::readMemory(int process_id, int virtual_address, uint16_t& value) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    if (page_tables.find(process_id) == page_tables.end()) {
        return false;
    }
    
    int page_number = virtual_address / mem_per_frame;
    
    if (page_number >= (int)page_tables[process_id].size()) {
        std::cerr << "ERROR: Invalid memory address.\n";
        return false;
    }
    
    // Check if page is in memory
    if (!page_tables[process_id][page_number].valid) {
        // Page fault
        if (!handlePageFault(process_id, page_number)) {
            return false;
        }
    }
    
    // Update access time
    int frame_id = page_tables[process_id][page_number].frame_number;
    frames[frame_id].last_access_time = getCurrentTime();
    page_tables[process_id][page_number].last_access_time = getCurrentTime();
    
    // Read value
    if (memory_data[process_id].find(virtual_address) != memory_data[process_id].end()) {
        value = memory_data[process_id][virtual_address];
    } else {
        value = 0; // Default value
    }
    
    return true;
}

bool MemoryManager::writeMemory(int process_id, int virtual_address, uint16_t value) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    if (page_tables.find(process_id) == page_tables.end()) {
        return false;
    }
    
    int page_number = virtual_address / mem_per_frame;
    
    if (page_number >= (int)page_tables[process_id].size()) {
        std::cerr << "ERROR: Invalid memory address.\n";
        return false;
    }
    
    // Check if page is in memory
    if (!page_tables[process_id][page_number].valid) {
        // Page fault
        if (!handlePageFault(process_id, page_number)) {
            return false;
        }
    }
    
    // Update access time
    int frame_id = page_tables[process_id][page_number].frame_number;
    frames[frame_id].last_access_time = getCurrentTime();
    page_tables[process_id][page_number].last_access_time = getCurrentTime();
    
    // Write value
    memory_data[process_id][virtual_address] = value;
    
    return true;
}

uint64_t MemoryManager::getCurrentTime() {
    return ++access_counter;
}

MemoryStats MemoryManager::getStats() {
    std::lock_guard<std::mutex> lock(memory_mutex);
    return stats;
}

void MemoryManager::printMemorySnapshot() {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    std::cout << "\n=== Memory Snapshot ===\n";
    std::cout << "Total Frames: " << total_frames << "\n";
    std::cout << "Used Frames: " << stats.used_frames << "\n";
    std::cout << "Free Frames: " << stats.free_frames << "\n";
    std::cout << "Total Page Faults: " << stats.total_page_faults << "\n";
    std::cout << "Pages In: " << stats.total_pages_in << "\n";
    std::cout << "Pages Out: " << stats.total_pages_out << "\n\n";
    
    std::cout << "Frame Allocation:\n";
    std::cout << "+-------+----------+----------+\n";
    std::cout << "| Frame | Process  | Page     |\n";
    std::cout << "+-------+----------+----------+\n";
    
    for (const auto& frame : frames) {
        std::cout << "| " << std::setw(5) << frame.frame_id << " | ";
        if (frame.is_free) {
            std::cout << std::setw(8) << "FREE" << " | ";
            std::cout << std::setw(8) << "-" << " |\n";
        } else {
            std::cout << std::setw(8) << frame.process_id << " | ";
            std::cout << std::setw(8) << frame.page_number << " |\n";
        }
    }
    std::cout << "+-------+----------+----------+\n";
}

int MemoryManager::getFrameForProcess(int process_id, int page_number) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    if (page_tables.find(process_id) == page_tables.end()) {
        return -1;
    }
    
    if (page_number >= (int)page_tables[process_id].size()) {
        return -1;
    }
    
    if (!page_tables[process_id][page_number].valid) {
        return -1;
    }
    
    return page_tables[process_id][page_number].frame_number;
}