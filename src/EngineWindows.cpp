#include "Engine.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <stdexcept>

namespace fs=std::filesystem;
namespace {
fs::path moduleDirectory() {
    HMODULE module=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&resourcePath),&module)) throw std::runtime_error("Cannot locate MITH Doom.dll");
    std::vector<wchar_t> path(32768);
    DWORD count=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));
    if (!count || count>=path.size()) throw std::runtime_error("Cannot locate plugin folder");
    return fs::path(path.data()).parent_path();
}
// Quote each Windows argument without involving a shell.
std::wstring quote(const std::wstring& value) {
    std::wstring result=L"\""; unsigned slashes=0;
    for(wchar_t c:value) {
        if(c==L'\\') { ++slashes; continue; }
        if(c==L'\"') result.append(slashes*2+1,L'\\');
        else result.append(slashes,L'\\');
        slashes=0; result+=c;
    }
    result.append(slashes*2,L'\\'); result+=L'\"'; return result;
}
struct Process {
    MDShared *shared=nullptr;
    HANDLE process=nullptr, job=nullptr, mapping=nullptr, file=INVALID_HANDLE_VALUE;
    fs::path dir;
    ~Process() { clear(); }
    void clear() {
        if(process) {
            if(shared) md_store32(&shared->quit,1);
            if(WaitForSingleObject(process,100)!=WAIT_OBJECT_0) { TerminateProcess(process,0); WaitForSingleObject(process,1000); }
            CloseHandle(process); process=nullptr;
        }
        if(job) { CloseHandle(job); job=nullptr; }
        if(shared) { UnmapViewOfFile(shared); shared=nullptr; }
        if(mapping) { CloseHandle(mapping); mapping=nullptr; }
        if(file!=INVALID_HANDLE_VALUE) { CloseHandle(file); file=INVALID_HANDLE_VALUE; }
        if(!dir.empty()) { std::error_code ec; fs::remove_all(dir,ec); dir.clear(); }
    }
    std::string log() {
        std::ifstream in(dir/L"engine.log"); std::string line,last;
        while(std::getline(in,line)) if(!line.empty()) last=line.substr(0,220);
        return last;
    }
    void launch(const std::string& wad) {
        for(unsigned attempt=0;attempt<10;++attempt) {
            auto candidate=fs::temp_directory_path()/(L"mith-doom-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(md_now()));
            std::error_code ec;
            if(fs::create_directory(candidate,ec)) { dir=candidate; break; }
        }
        if(dir.empty()) throw std::runtime_error("Cannot create engine directory");
        fs::path ipc=dir/L"frames";
        file=CreateFileW(ipc.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create frame buffer");
        LARGE_INTEGER length; length.QuadPart=sizeof(MDShared);
        if(!SetFilePointerEx(file,length,nullptr,FILE_BEGIN) || !SetEndOfFile(file)) throw std::runtime_error("Cannot size frame buffer");
        mapping=CreateFileMappingW(file,nullptr,PAGE_READWRITE,0,0,nullptr);
        if(!mapping) throw std::runtime_error("Cannot map frame buffer");
        shared=static_cast<MDShared*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(MDShared)));
        if(!shared) throw std::runtime_error("Cannot access frame buffer");
        std::memset(shared,0,sizeof(MDShared)); shared->magic=MD_MAGIC; md_store64(&shared->heartbeat,md_now());
        job=CreateJobObjectW(nullptr,nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{}; limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!job || !SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))
            throw std::runtime_error("Cannot create engine process guard");
        fs::path exe=fs::u8path(resourcePath("DoomEngine.exe"));
        auto cmd=quote(exe.wstring())+L" "+quote(ipc.wstring())+L" "+quote(fs::u8path(wad).wstring())+L" "+std::to_wstring(GetCurrentProcessId());
        STARTUPINFOW startup{}; startup.cb=sizeof(startup); PROCESS_INFORMATION pi{};
        if(!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,
                nullptr,dir.c_str(),&startup,&pi)) throw std::runtime_error("Cannot start DoomEngine.exe (error "+std::to_string(GetLastError())+")");
        process=pi.hProcess;
        if(!AssignProcessToJobObject(job,process)) { CloseHandle(pi.hThread); throw std::runtime_error("Cannot guard engine process"); }
        if(ResumeThread(pi.hThread)==DWORD(-1)) { CloseHandle(pi.hThread); throw std::runtime_error("Cannot resume engine process"); }
        CloseHandle(pi.hThread);
    }
};
void validateWad(const std::string& path) {
    std::ifstream file(fs::u8path(path),std::ios::binary);
    if(!file) throw std::runtime_error("WAD file not found");
    char header[12]{}; file.read(header,12);
    if(file.gcount()!=12 || std::memcmp(header,"IWAD",4)) throw std::runtime_error("Choose a Doom IWAD (not an add-on PWAD)");
    uint32_t count,offset; std::memcpy(&count,header+4,4); std::memcpy(&offset,header+8,4);
    file.seekg(0,std::ios::end); auto length=file.tellg();
    if(!count || count>100000 || uint64_t(offset)+uint64_t(count)*16>uint64_t(length)) throw std::runtime_error("Invalid WAD directory");
}
}
std::string resourcePath(const char *name) { return (moduleDirectory()/L"MITH Doom Resources"/fs::u8path(name)).u8string(); }
std::string defaultWadPath() {
    auto effects=moduleDirectory();
    for(const auto& path:{effects/L"MITH Doom Data"/L"doom1.wad",effects.parent_path()/L"samples"/L"doom1.wad"}) {
        std::error_code ec; if(fs::is_regular_file(path,ec)) return path.u8string();
    }
    return {};
}
void Engine::start() { if(!worker_.joinable()) { stopping_=false; worker_=std::thread(&Engine::run,this); } }
void Engine::stop() { stopping_=true; if(worker_.joinable()) worker_.join(); }
void Engine::request(std::string path) { std::lock_guard<std::mutex> lock(mutex_); path_=std::move(path); ++request_; }
void Engine::controls(uint32_t keys,bool paused) { std::lock_guard<std::mutex> lock(mutex_); keys_=keys; paused_=paused; heartbeat_=md_now(); }
Engine::Snapshot Engine::snapshot() { std::lock_guard<std::mutex> lock(mutex_); return result_; }
void Engine::run() {
    Process child; uint64_t handled=0,launchTime=0; uint32_t sequence=0;
    auto status=[&](std::string msg,bool blank=false) { std::lock_guard<std::mutex> lock(mutex_); result_.status=std::move(msg); if(blank) { result_.pixels.fill(0); ++result_.serial; } };
    while(!stopping_) {
        std::string path; uint64_t request,heartbeat; uint32_t keys; bool paused;
        { std::lock_guard<std::mutex> lock(mutex_); path=path_; request=request_; keys=keys_; paused=paused_; heartbeat=heartbeat_; }
        if(request!=handled) {
            handled=request; child.clear(); sequence=0; status("Starting...",true);
            if(path.empty()) status("Choose a Doom WAD in Game > WAD File");
            else try { validateWad(path); child.launch(path); launchTime=md_now(); }
            catch(const std::exception& e) { child.clear(); status(e.what()); }
        }
        if(child.process) {
            md_store32(&child.shared->keys,keys); md_store32(&child.shared->paused,paused?1u:0u); md_store64(&child.shared->heartbeat,heartbeat);
            for(auto& f:child.shared->frames) {
                if(!md_claim(&f.state,2,3)) continue;
                if(f.sequence>sequence) {
                    sequence=f.sequence; std::lock_guard<std::mutex> lock(mutex_);
                    std::copy(std::begin(f.pixels),std::end(f.pixels),result_.pixels.begin());
                    ++result_.serial; result_.status="Playing: "+fs::u8path(path).filename().u8string();
                }
                md_store32(&f.state,0);
            }
            if(WaitForSingleObject(child.process,0)==WAIT_OBJECT_0) {
                auto detail=child.log(); status("Engine stopped. Restart or choose another WAD."+(detail.empty()?"":" "+detail)); child.clear();
            } else if(!sequence && md_now()-launchTime>15000000000ull) { status("Engine did not start. Choose a supported Doom WAD."); child.clear(); }
        }
        md_sleep(5);
    }
}
