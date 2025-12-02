#include "../include/MemoryManager.h"
#include "../include/Config.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <sstream>

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
        // Initialize physical memory for this frame with 0s
        frames[i].data.assign(mem_per_frame, 0);
    }

    // Clear backing store file
    std::ofstream ofs("csopesy-backing-store.txt", std::ofstream::out | std::ofstream::trunc);
    ofs.close();
    backing_store_disk.clear();

    stats.total_frames = total_frames;
    stats.used_frames = 0;
    stats.free_frames = total_frames;
    stats.total_page_faults = 0;

    initialized = true;

    std::cout << "Memory Manager initialized: " << total_frames << " frames, "
        << mem_per_frame << " bytes per frame.\n";
}

bool MemoryManager::allocateMemory(int process_id, int process_memory_size) {
    std::lock_guard<std::mutex> lock(memory_mutex);

    if (!initialized) return false;

    // Calculate pages needed
    int num_pages = process_memory_size / mem_per_frame;
    if (process_memory_size % mem_per_frame != 0) num_pages++;

    // Initialize page table
    page_tables[process_id].resize(num_pages);
    for (int i = 0; i < num_pages; i++) {
        page_tables[process_id][i].valid = false;
        page_tables[process_id][i].frame_number = -1;
    }

    process_memory_sizes[process_id] = process_memory_size;
    return true;
}

void MemoryManager::deallocateMemory(int process_id) {
    std::lock_guard<std::mutex> lock(memory_mutex);

    // [DEBUG] Print to confirm deallocation is requested
    // std::cout << "DEBUG: Deallocating Memory for PID " << process_id << "\n";

    // 1. Free all frames used by this process (Physical Memory)
    for (auto& frame : frames) {
        if (frame.process_id == process_id) {
            frame.is_free = true;
            frame.process_id = -1;
            frame.page_number = -1;
            std::fill(frame.data.begin(), frame.data.end(), 0);

            // Update stats immediately
            if (stats.used_frames > 0) stats.used_frames--;
            if (stats.free_frames < total_frames) stats.free_frames++;
        }
    }

    // 2. Remove from Page Tables (Virtual Memory)
    if (page_tables.find(process_id) != page_tables.end()) {
        page_tables.erase(process_id);
    }

    // 3. Remove Memory Size tracking
    if (process_memory_sizes.find(process_id) != process_memory_sizes.end()) {
        process_memory_sizes.erase(process_id);
    }

    // 4. Clean backing store (Disk / Swap)
    bool disk_updated = false;
    for (auto it = backing_store_disk.begin(); it != backing_store_disk.end(); ) {
        // Key format is "PID_PageNum"
        // Check if key starts with "PID_"
        if (it->first.find(std::to_string(process_id) + "_") == 0) {
            it = backing_store_disk.erase(it);
            disk_updated = true;
        }
        else {
            ++it;
        }
    }   

    if (disk_updated) {
        std::ofstream ofs("csopesy-backing-store.txt");
        if (ofs.is_open()) {
            for (const auto& pair : backing_store_disk) {
                ofs << "Key: " << pair.first << " Data: [";
                for (size_t i = 0; i < pair.second.size(); ++i) {
                    ofs << pair.second[i] << (i < pair.second.size() - 1 ? " " : "");
                }
                ofs << "]\n";
            }
            ofs.close();
        }
    }
}
int MemoryManager::findFreeFrame() {
    for (int i = 0; i < total_frames; i++) {
        if (frames[i].is_free) {
            return i;
        }
    }
    return -1;
}

void MemoryManager::saveFrameToBackingStore(int frame_id, int process_id, int page_num) {
    std::string key = std::to_string(process_id) + "_" + std::to_string(page_num);
    backing_store_disk[key] = frames[frame_id].data;

    // Update text file
    std::ofstream ofs("csopesy-backing-store.txt");
    if (ofs.is_open()) {
        for (const auto& pair : backing_store_disk) {
            ofs << "Key: " << pair.first << " Data: [";
            for (size_t i = 0; i < pair.second.size(); ++i) {
                ofs << pair.second[i] << (i < pair.second.size() - 1 ? " " : "");
            }
            ofs << "]\n";
        }
        ofs.close();
    }
}

bool MemoryManager::loadFrameFromBackingStore(int frame_id, int process_id, int page_num) {
    std::string key = std::to_string(process_id) + "_" + std::to_string(page_num);

    if (backing_store_disk.find(key) != backing_store_disk.end()) {
        frames[frame_id].data = backing_store_disk[key];
        return true;
    }
    else {
        std::fill(frames[frame_id].data.begin(), frames[frame_id].data.end(), 0);
        return false;
    }
}

int MemoryManager::evictPage() {
    uint64_t oldest_time = UINT64_MAX;
    int victim_frame_index = -1;

    for (int i = 0; i < total_frames; i++) {
        // [FIX]: Ensure we only check currently occupied frames
        if (!frames[i].is_free && frames[i].last_access_time < oldest_time) {
            oldest_time = frames[i].last_access_time;
            victim_frame_index = i;
        }
    }

    if (victim_frame_index != -1) {
        Frame& frame = frames[victim_frame_index];

        // Swap Out
        if (frame.process_id != -1 && frame.page_number != -1) {
            saveFrameToBackingStore(victim_frame_index, frame.process_id, frame.page_number);
        }

        // Invalidate victim page table
        if (page_tables.count(frame.process_id)) {
            auto& pt = page_tables[frame.process_id];
            if (frame.page_number < (int)pt.size()) {
                pt[frame.page_number].valid = false;
                pt[frame.page_number].frame_number = -1;
            }
        }

        stats.total_pages_out++;

        // Temporarily mark free so handlePageFault can grab it
        frame.is_free = true;
        frame.process_id = -1;
        frame.page_number = -1;

        if (stats.used_frames > 0) stats.used_frames--;
        if (stats.free_frames < total_frames) stats.free_frames++;
    }

    return victim_frame_index;
}

bool MemoryManager::handlePageFault(int process_id, int page_number) {
    if (page_tables.find(process_id) == page_tables.end()) return false;
    if (page_number >= (int)page_tables[process_id].size()) return false;

    int frame_id = findFreeFrame();

    if (frame_id == -1) {
        frame_id = evictPage();
        if (frame_id == -1) {
            std::cerr << "CRITICAL ERROR: Failed to find or evict a frame!\n";
            return false;
        }
    }

    // Allocate Frame
    frames[frame_id].is_free = false;
    frames[frame_id].process_id = process_id;
    frames[frame_id].page_number = page_number;
    frames[frame_id].last_access_time = getCurrentTime();

    // Swap In
    bool was_paged_in = loadFrameFromBackingStore(frame_id, process_id, page_number);

    // Update Page Table
    page_tables[process_id][page_number].valid = true;
    page_tables[process_id][page_number].frame_number = frame_id;
    page_tables[process_id][page_number].last_access_time = getCurrentTime();

    stats.used_frames++;
    stats.free_frames--;
    stats.total_page_faults++;

    if (was_paged_in) {
        stats.total_pages_in++;  // Only count actual disk reads
    }


    return true;
}

bool MemoryManager::readMemory(int process_id, int virtual_address, uint16_t& value) {
    std::lock_guard<std::mutex> lock(memory_mutex);

    if (page_tables.find(process_id) == page_tables.end()) return false;

    int page_number = virtual_address / mem_per_frame;
    int offset = virtual_address % mem_per_frame;

    if (page_number >= (int)page_tables[process_id].size()) return false;

    if (!page_tables[process_id][page_number].valid) {
        if (!handlePageFault(process_id, page_number)) return false;
    }

    int frame_id = page_tables[process_id][page_number].frame_number;

    // Update LRU
    frames[frame_id].last_access_time = getCurrentTime();
    page_tables[process_id][page_number].last_access_time = getCurrentTime();

    // Read Data
    if (offset < frames[frame_id].data.size()) {
        value = frames[frame_id].data[offset];
        return true;
    }

    return false; // Offset out of bounds
}

bool MemoryManager::writeMemory(int process_id, int virtual_address, uint16_t value) {
    std::lock_guard<std::mutex> lock(memory_mutex);

    if (page_tables.find(process_id) == page_tables.end()) {
        std::cerr << "Write Fail: PID " << process_id << " not found in page tables.\n";
        return false;
    }

    int page_number = virtual_address / mem_per_frame;
    int offset = virtual_address % mem_per_frame;

    if (page_number >= (int)page_tables[process_id].size()) {
        std::cerr << "Write Fail: Page " << page_number << " out of bounds.\n";
        return false;
    }

    if (!page_tables[process_id][page_number].valid) {
        if (!handlePageFault(process_id, page_number)) {
            std::cerr << "Write Fail: Page Fault handling failed.\n";
            return false;
        }
    }

    int frame_id = page_tables[process_id][page_number].frame_number;

    // Update LRU
    frames[frame_id].last_access_time = getCurrentTime();
    page_tables[process_id][page_number].last_access_time = getCurrentTime();

    // Write Data
    if (offset < frames[frame_id].data.size()) {
        frames[frame_id].data[offset] = value;
        // DEBUG PRINT
        // std::cout << "Debug: Wrote " << value << " to Frame " << frame_id << " offset " << offset << "\n";
        return true;
    }
    else {
        std::cerr << "Write Fail: Offset " << offset << " out of frame bounds (" << frames[frame_id].data.size() << ")\n";
        return false;
    }
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
    std::cout << "Used Frames: " << stats.used_frames << " / " << total_frames << "\n";
    std::cout << "Page Faults: " << stats.total_page_faults << "\n";
    std::cout << "+-------+----------+----------+\n";
    std::cout << "| Frame | Process  | Page     |\n";
    std::cout << "+-------+----------+----------+\n";
    for (const auto& frame : frames) {
        std::cout << "| " << std::setw(5) << frame.frame_id << " | ";
        if (frame.is_free) std::cout << std::setw(8) << "FREE" << " | " << std::setw(8) << "-" << " |\n";
        else std::cout << std::setw(8) << frame.process_id << " | " << std::setw(8) << frame.page_number << " |\n";
    }
    std::cout << "+-------+----------+----------+\n";
}

int MemoryManager::getFrameForProcess(int process_id, int page_number) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    if (page_tables.count(process_id) && page_number < page_tables[process_id].size() && page_tables[process_id][page_number].valid)
        return page_tables[process_id][page_number].frame_number;
    return -1;
}
