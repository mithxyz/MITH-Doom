// Actual Windows DLL + helper smoke test. Run on Windows or under Wine.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/glew.h>
#include <FFGLSDK.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>

namespace fs=std::filesystem;
using Entry=FFMixed (__stdcall*)(FFUInt32,FFMixed,FFInstanceID);
Entry entry;
FFInstanceID instance;
void check(bool condition,const char *message) { if(!condition) throw std::runtime_error(message); }
FFMixed call(unsigned op,unsigned value=0,FFInstanceID id=nullptr) { FFMixed input{}; input.UIntValue=value; return entry(op,input,id); }
void set(unsigned p,float value) {
    SetParameterStruct param{}; param.ParameterNumber=p; std::memcpy(&param.NewParameterValue.UIntValue,&value,4);
    FFMixed input{}; input.PointerValue=&param;
    check(entry(8,input,instance).UIntValue==0,"Set float parameter failed");
}
void file(const std::string& path) {
    SetParameterStruct param{}; param.ParameterNumber=0; param.NewParameterValue.PointerValue=const_cast<char*>(path.c_str());
    FFMixed input{}; input.PointerValue=&param;
    check(entry(8,input,instance).UIntValue==0,"Set WAD parameter failed");
}
std::string status() { return static_cast<const char*>(call(9,2,instance).PointerValue); }
GLuint fbo;
std::vector<unsigned char> render() {
    ProcessOpenGLStruct process{}; process.HostFBO=fbo;
    FFMixed input{}; input.PointerValue=&process;
    check(entry(17,input,instance).UIntValue==0,"FFGL rendering failed");
    check(glGetError()==GL_NO_ERROR,"OpenGL error");
    std::vector<unsigned char> result(640*480*4);
    glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,result.data()); return result;
}
std::vector<unsigned char> run(unsigned ms) {
    ULONGLONG end=GetTickCount64()+ms;
    std::vector<unsigned char> frame;
    do { frame=render(); Sleep(15); } while(GetTickCount64()<end);
    return frame;
}
void wait(const char *prefix) {
    ULONGLONG end=GetTickCount64()+15000;
    do { render(); if(status().rfind(prefix,0)==0) return; Sleep(15); } while(GetTickCount64()<end);
    throw std::runtime_error(status());
}
void save(const fs::path& path,const std::vector<unsigned char>& data) {
    std::ofstream out(path,std::ios::binary); out<<"P6\n640 480\n255\n";
    for(int y=479;y>=0;--y) for(int x=0;x<640;++x) out.write(reinterpret_cast<const char*>(&data[(y*640+x)*4]),3);
}
int wmain(int argc,wchar_t **argv) {
    try {
        check(argc==4,"Usage: smoke_windows.exe plugin.dll game.wad output-directory");
        fs::create_directories(argv[3]);
        HMODULE lib=LoadLibraryW(argv[1]); check(lib!=nullptr,"Cannot load Windows plugin DLL");
        entry=reinterpret_cast<Entry>(GetProcAddress(lib,"plugMain")); check(entry!=nullptr,"Missing FFGL entry point");
        check(call(34).UIntValue==0,"Cannot initialise FFGL DLL");
        check(call(4).UIntValue==30,"Parameter count");
        check(call(15,0).UIntValue==14,"Native WAD parameter");
        check(std::string(static_cast<const char*>(call(6,29).PointerValue))=="https://MITH.xyz","Brand default");
        char group[64]{}; GetStringStruct query{29,{group,63}}; FFMixed groupInput{}; groupInput.PointerValue=&query;
        check(entry(50,groupInput,nullptr).UIntValue==0 && std::string(group)=="MITH.xyz","Brand control group");
        WNDCLASSW wc{}; wc.style=CS_OWNDC; wc.lpfnWndProc=DefWindowProcW; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"MITHDoomSmoke";
        RegisterClassW(&wc);
        HWND window=CreateWindowW(wc.lpszClassName,L"MITH Doom validation",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,wc.hInstance,nullptr);
        check(window!=nullptr,"Cannot create Windows GL test window"); HDC dc=GetDC(window);
        PIXELFORMATDESCRIPTOR format{}; format.nSize=sizeof(format); format.nVersion=1; format.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
        format.iPixelType=PFD_TYPE_RGBA; format.cColorBits=32; format.cAlphaBits=8;
        check(SetPixelFormat(dc,ChoosePixelFormat(dc,&format),&format)!=0,"Cannot choose GL pixel format");
        HGLRC legacy=wglCreateContext(dc); check(legacy && wglMakeCurrent(dc,legacy),"Cannot create GL context");
        using CreateContext=HGLRC (WINAPI*)(HDC,HGLRC,const int*);
        auto create=reinterpret_cast<CreateContext>(wglGetProcAddress("wglCreateContextAttribsARB")); check(create!=nullptr,"No modern OpenGL context support");
        int attrs[]={0x2091,4,0x2092,1,0x9126,1,0}; HGLRC context=create(dc,nullptr,attrs); check(context && wglMakeCurrent(dc,context),"Cannot create OpenGL 4.1 core context");
        wglDeleteContext(legacy); glewExperimental=GL_TRUE; check(glewInit()==GLEW_OK,"GLEW load failed"); while(glGetError()!=GL_NO_ERROR) {}
        std::cout<<"Renderer: "<<glGetString(GL_RENDERER)<<"\n";
        GLuint texture; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,640,480,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Incomplete host FBO"); glViewport(0,0,640,480);
        FFGLViewportStruct vp{0,0,640,480}; FFMixed input{}; input.PointerValue=&vp;
        instance=entry(18,input,nullptr).PointerValue; check(instance && instance!=reinterpret_cast<void*>(uintptr_t(0xffffffff)),"Cannot instantiate Windows source");
        file(fs::path(argv[2]).u8string()); wait("Playing:"); auto base=run(2500); save(fs::path(argv[3])/L"windows-gameplay.ppm",base);
        set(26,1); auto frozen=run(150); check(frozen==run(250),"Pause frame changed");
        set(26,0); set(3,1); run(400); set(3,0); set(26,1); auto moved=run(150); check(moved!=frozen,"Forward did not move");
        set(26,0); set(6,1); run(300); set(6,0); set(26,1); auto turned=run(150); check(turned!=moved,"Turn did not move");
        set(26,0); set(1,1); wait("Starting"); wait("Playing:"); auto restarted=run(2500);
        set(9,1); auto shot=run(250); set(9,0); save(fs::path(argv[3])/L"windows-firing.ppm",shot); check(shot!=restarted,"Fire did not change frame");
        set(12,1); auto menu=run(150); set(12,0); check(menu!=shot,"Menu did not open"); save(fs::path(argv[3])/L"windows-menu.ppm",menu);
        file(fs::path(argv[3]).u8string()+"/missing.wad"); wait("WAD file not found");
        file(fs::path(argv[2]).u8string()); wait("Playing:"); run(100);
        check(call(19,0,instance).UIntValue==0,"Instance cleanup failed"); instance=nullptr;
        check(call(2).UIntValue==0,"DLL cleanup failed");
        FreeLibrary(lib); wglMakeCurrent(nullptr,nullptr); wglDeleteContext(context); ReleaseDC(window,dc); DestroyWindow(window);
        std::cout<<"PASS: Windows DLL metadata, MITH.xyz brand, WAD loading, rendered gameplay, movement, pause, restart, firing, menu, missing-file recovery, cleanup.\n";
        return 0;
    } catch(const std::exception& error) {
        if(instance) { call(19,0,instance); instance=nullptr; }
        std::cerr<<"FAIL: "<<error.what()<<"\n"; return 1;
    }
}
