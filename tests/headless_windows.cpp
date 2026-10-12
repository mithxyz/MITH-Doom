// Native Windows process/IPC/gameplay checks without requiring a GPU.
#include "Engine.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <cstring>
namespace fs=std::filesystem;
union Mixed { uint32_t UIntValue; void *PointerValue; };
using Entry=Mixed (__stdcall*)(uint32_t,Mixed,void*);
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
Engine::Snapshot run(Engine& engine,unsigned ms,uint32_t keys=0,bool pause=false) {
    auto end=md_now()+uint64_t(ms)*1000000;
    do { engine.controls(keys,pause); md_sleep(10); } while(md_now()<end);
    return engine.snapshot();
}
void wait(Engine& engine,const char *prefix) {
    auto end=md_now()+15000000000ull;
    do { engine.controls(0,false); auto frame=engine.snapshot(); if(frame.status.rfind(prefix,0)==0) return; md_sleep(10); } while(md_now()<end);
    throw std::runtime_error(engine.snapshot().status);
}
void save(const fs::path& path,const Engine::Snapshot& frame) {
    std::ofstream out(path,std::ios::binary); out<<"P6\n320 200\n255\n";
    for(auto p:frame.pixels) { char rgb[]={char(p>>16),char(p>>8),char(p)}; out.write(rgb,3); }
}
int wmain(int argc,wchar_t **argv) {
    try {
        check(argc==4,"Usage: headless_windows.exe plugin.dll game.wad output-folder");
        fs::create_directories(argv[3]);
        HMODULE lib=LoadLibraryW(argv[1]); check(lib!=nullptr,"DLL did not load");
        auto entry=reinterpret_cast<Entry>(GetProcAddress(lib,"plugMain")); check(entry!=nullptr,"Missing plugMain");
        check(entry(34,Mixed{0},nullptr).UIntValue==0,"DLL metadata initialise");
        check(entry(4,Mixed{0},nullptr).UIntValue==30,"Parameter count");
        check(entry(15,Mixed{0},nullptr).UIntValue==14,"WAD parameter type");
        check(std::string(static_cast<const char*>(entry(6,Mixed{29},nullptr).PointerValue))=="https://MITH.xyz","Brand field");
        check(entry(2,Mixed{0},nullptr).UIntValue==0,"DLL metadata cleanup"); FreeLibrary(lib);
        auto wad=fs::path(argv[2]).u8string();
        Engine engine; engine.request(wad); engine.start(); wait(engine,"Playing:"); auto base=run(engine,2500); save(fs::path(argv[3])/L"windows-engine-gameplay.ppm",base);
        unsigned lit=0; for(auto p:base.pixels) lit+=p!=0;
        check(lit>40000,"No game framebuffer");
        auto frozen=run(engine,150,0,true); check(frozen.pixels==run(engine,200,0,true).pixels,"Pause not stable");
        run(engine,400,1u<<0); auto moved=run(engine,150,0,true); check(moved.pixels!=frozen.pixels,"Forward did not move");
        run(engine,300,1u<<3); auto turned=run(engine,150,0,true); check(turned.pixels!=moved.pixels,"Turn did not move");
        engine.request(wad); wait(engine,"Starting"); wait(engine,"Playing:"); auto initial=run(engine,2500);
        auto firing=run(engine,250,1u<<6); run(engine,450); auto shot=run(engine,150,0,true);
        bool ammo=false; for(int y=172;y<192;++y) for(int x=0;x<42;++x) ammo|=initial.pixels[y*320+x]!=shot.pixels[y*320+x];
        check(ammo,"Fire did not reduce ammo HUD"); save(fs::path(argv[3])/L"windows-engine-firing.ppm",firing);
        auto menu=run(engine,150,1u<<9); run(engine,80); check(menu.pixels!=shot.pixels,"Menu did not open"); save(fs::path(argv[3])/L"windows-engine-menu.ppm",menu);
        Engine other; other.request(wad); other.start(); wait(other,"Playing:"); run(other,2500); auto a=run(other,150,0,true);
        run(engine,150,1u<<3); check(a.pixels==run(other,150,0,true).pixels,"Independent instances"); other.stop();
        engine.request(fs::path(argv[3]).u8string()+"/missing.wad"); wait(engine,"WAD file not found");
        fs::path bad=fs::path(argv[3])/L"windows-corrupt.wad";
        { std::ofstream out(bad,std::ios::binary); out.write("IWAD",4); char zeros[8]{}; out.write(zeros,8); }
        engine.request(bad.u8string()); wait(engine,"Invalid WAD directory"); fs::remove(bad);
        engine.request(wad); wait(engine,"Playing:"); run(engine,150); engine.stop();
        std::cout<<"PASS: x64 DLL loads, branded FFGL metadata, Windows helper/process guard, shared memory, WAD loading, real engine frames, movement, pause, restart, firing/ammo HUD, menus, independent instances, missing/corrupt WAD recovery, cleanup.\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
}
