#include "Engine.h"
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <cerrno>
#include <spawn.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
extern char **environ;

std::string resourcePath(const char *name) {
    Dl_info loc{};
    if (dladdr(reinterpret_cast<void*>(&resourcePath), &loc) && loc.dli_fname)
        return (std::filesystem::path(loc.dli_fname).parent_path().parent_path()/"Resources"/name).string();
    return {};
}
std::string defaultWadPath() {
    auto effects = std::filesystem::path(resourcePath("DoomEngine")).parent_path().parent_path().parent_path().parent_path();
    for (const auto& path : {effects/"MITH Doom Data"/"doom1.wad", effects.parent_path()/"samples"/"doom1.wad"}) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(path,ec)) return path.string();
    }
    return {};
}
namespace {
struct Process {
    MDShared *shared = nullptr;
    pid_t pid = 0;
    std::string dir;
    ~Process() { clear(); }
    void clear() {
        if (pid > 0) {
            if (shared) md_store32(&shared->quit, 1);
            kill(pid, SIGTERM);
            int status;
            for (int i = 0; i < 20; ++i) {
                pid_t r = waitpid(pid, &status, WNOHANG);
                if (r == pid || (r < 0 && errno == ECHILD)) { pid = 0; break; }
                usleep(5000);
            }
            if (pid > 0) { kill(pid, SIGKILL); while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {} pid = 0; }
        }
        if (shared) { munmap(shared, sizeof(MDShared)); shared = nullptr; }
        if (!dir.empty()) { std::error_code ec; std::filesystem::remove_all(dir, ec); dir.clear(); }
    }
    std::string log() {
        std::ifstream in(dir + "/engine.log");
        std::string line, last;
        while (std::getline(in, line)) if (!line.empty()) last = line.substr(0, 220);
        return last;
    }
    void launch(const std::string& wad) {
        auto tmp = std::filesystem::temp_directory_path()/"mith-doom-XXXXXX";
        std::string pattern = tmp.string();
        if (!mkdtemp(pattern.data())) throw std::runtime_error("Cannot create engine directory");
        dir = pattern;
        std::string ipc = dir + "/frames";
        int fd = open(ipc.c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
        if (fd < 0) throw std::runtime_error("Cannot create frame buffer");
        if (ftruncate(fd, sizeof(MDShared))) { close(fd); throw std::runtime_error("Cannot size frame buffer"); }
        void *mapped = mmap(nullptr, sizeof(MDShared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
        if (mapped == MAP_FAILED) throw std::runtime_error("Cannot map frame buffer");
        shared = static_cast<MDShared*>(mapped);
        std::memset(shared, 0, sizeof(MDShared)); shared->magic = MD_MAGIC;
        md_store64(&shared->heartbeat, md_now());
        std::string exe = resourcePath("DoomEngine"), parent = std::to_string(getpid());
        std::vector<char*> args = {exe.data(), ipc.data(), const_cast<char*>(wad.c_str()), parent.data(), nullptr};
        posix_spawn_file_actions_t actions; posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_addopen(&actions, 1, (dir+"/engine.log").c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        posix_spawn_file_actions_adddup2(&actions, 1, 2);
        posix_spawnattr_t attr; posix_spawnattr_init(&attr);
        posix_spawnattr_setflags(&attr, POSIX_SPAWN_CLOEXEC_DEFAULT);
        int error = posix_spawn(&pid, exe.c_str(), &actions, &attr, args.data(), environ);
        posix_spawnattr_destroy(&attr); posix_spawn_file_actions_destroy(&actions);
        if (error) { pid = 0; throw std::runtime_error("Cannot start DoomEngine: " + std::string(std::strerror(error))); }
    }
};
void validateWad(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("WAD file not found");
    char header[12]{}; file.read(header, 12);
    if (file.gcount() != 12 || std::memcmp(header, "IWAD", 4)) throw std::runtime_error("Choose a Doom IWAD (not an add-on PWAD)");
    // Engine failures remain confined to the helper; reject obviously corrupt headers early.
    uint32_t count, offset; std::memcpy(&count, header+4, 4); std::memcpy(&offset, header+8, 4);
    file.seekg(0, std::ios::end); auto length = file.tellg();
    if (!count || count > 100000 || uint64_t(offset) + uint64_t(count)*16 > uint64_t(length))
        throw std::runtime_error("Invalid WAD directory");
}
}
void Engine::start() { if (!worker_.joinable()) { stopping_ = false; worker_ = std::thread(&Engine::run, this); } }
void Engine::stop() { stopping_ = true; if (worker_.joinable()) worker_.join(); }
void Engine::request(std::string path) { std::lock_guard<std::mutex> lock(mutex_); path_ = std::move(path); ++request_; }
void Engine::controls(uint32_t keys, bool paused) { std::lock_guard<std::mutex> lock(mutex_); keys_ = keys; paused_ = paused; heartbeat_ = md_now(); }
Engine::Snapshot Engine::snapshot() { std::lock_guard<std::mutex> lock(mutex_); return result_; }
void Engine::run() {
    Process child;
    uint64_t handled = 0, launchTime = 0;
    uint32_t sequence = 0;
    auto status = [&](std::string msg, bool blank=false) {
        std::lock_guard<std::mutex> lock(mutex_); result_.status = std::move(msg);
        if (blank) { result_.pixels.fill(0); ++result_.serial; }
    };
    while (!stopping_) {
        std::string path; uint64_t request, heartbeat; uint32_t keys; bool paused;
        { std::lock_guard<std::mutex> lock(mutex_); path=path_; request=request_; keys=keys_; paused=paused_; heartbeat=heartbeat_; }
        if (request != handled) {
            handled=request; child.clear(); sequence=0; status("Starting...", true);
            if (path.empty()) status("Choose a Doom WAD in Game > WAD File");
            else try { validateWad(path); child.launch(path); launchTime=md_now(); }
            catch (const std::exception& e) { child.clear(); status(e.what()); }
        }
        if (child.pid > 0) {
            md_store32(&child.shared->keys, keys);
            md_store32(&child.shared->paused, paused ? 1u : 0u);
            md_store64(&child.shared->heartbeat, heartbeat);
            for (auto& f : child.shared->frames) {
                if (!md_claim(&f.state, 2, 3)) continue;
                if (f.sequence > sequence) {
                    sequence=f.sequence;
                    std::lock_guard<std::mutex> lock(mutex_);
                    std::copy(std::begin(f.pixels), std::end(f.pixels), result_.pixels.begin());
                    ++result_.serial; result_.status="Playing: " + std::filesystem::path(path).filename().string();
                }
                md_store32(&f.state, 0);
            }
            int code; pid_t r = waitpid(child.pid, &code, WNOHANG);
            if (r == child.pid) {
                child.pid=0; auto detail=child.log();
                status("Engine stopped. Restart or choose another WAD." + (detail.empty()?"":" " + detail)); child.clear();
            } else if (!sequence && md_now()-launchTime > 15000000000ull) {
                status("Engine did not start. Choose a supported Doom WAD."); child.clear();
            }
        }
        usleep(5000);
    }
}
