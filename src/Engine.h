#pragma once
#include "Protocol.h"
#include <array>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

class Engine {
public:
    struct Snapshot {
        std::array<uint32_t, MD_PIXELS> pixels{};
        uint64_t serial = 0;
        std::string status;
    };
    ~Engine() { stop(); }
    void start();
    void stop();
    void request(std::string path);
    void controls(uint32_t keys, bool paused);
    Snapshot snapshot();
private:
    void run();
    std::thread worker_;
    std::atomic<bool> stopping_{false};
    std::mutex mutex_;
    std::string path_;
    uint64_t request_ = 1, heartbeat_ = 0;
    uint32_t keys_ = 0;
    bool paused_ = false;
    Snapshot result_;
};
std::string resourcePath(const char *name);
std::string defaultWadPath();
