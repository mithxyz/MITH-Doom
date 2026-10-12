#include "Doom.h"
#include "GLState.h"
#include "Splash.h"
#include <cmath>
#include <algorithm>

static CFFGLPluginInfo info(PluginFactory<Doom>, "MDOM", "MITH Doom", 2, 1, 0, 101,
    FF_SOURCE, "Playable silent Doom. Select a WAD; map controls with Piano keyboard shortcuts.",
    "MITH.xyz | MITH Doom 0.1.1 | DoomGeneric | Mac + Windows");
static const char *vertex = R"(#version 410 core
out vec2 uv;
void main() {
    vec2 p=vec2((gl_VertexID<<1)&2, gl_VertexID&2);
    uv=p; gl_Position=vec4(p*2.0-1.0,0,1);
})";
static const char *fragment = R"(#version 410 core
in vec2 uv; out vec4 colour;
uniform sampler2D Image;
uniform vec2 Resolution;
uniform int Fit;
void main() {
    vec2 pos=uv;
    if (Fit!=0) {
        float aspect=Resolution.x/Resolution.y;
        vec2 extent=aspect>4.0/3.0 ? vec2((4.0/3.0)/aspect,1) : vec2(1,aspect/(4.0/3.0));
        pos=(uv-0.5)/extent+0.5;
    }
    if (any(lessThan(pos,vec2(0))) || any(greaterThan(pos,vec2(1)))) { colour=vec4(0,0,0,1); return; }
    colour=vec4(texture(Image,vec2(pos.x,1.0-pos.y)).rgb,1);
})";
void Doom::button(unsigned p, const char* name, const char* group) {
    SetParamInfo(p,name,FF_TYPE_BOOLEAN,false); SetParamGroup(p,group);
}
Doom::Doom() {
    SetMinInputs(0); SetMaxInputs(0);
    file_=defaultWadPath();
    SetFileParamInfo(File,"WAD File",{"wad"},file_.c_str()); SetParamGroup(File,"Game");
    SetParamInfo(Restart,"Restart",FF_TYPE_EVENT,false); SetParamGroup(Restart,"Game");
    SetParamInfo(Status,"Status",FF_TYPE_TEXT,status_.c_str()); SetParamGroup(Status,"Game");
    button(Forward,"Forward / Up","Movement"); button(Back,"Backward / Down","Movement");
    button(Left,"Turn Left","Movement"); button(Right,"Turn Right","Movement");
    button(StrafeLeft,"Strafe Left","Movement"); button(StrafeRight,"Strafe Right","Movement");
    button(Run,"Run","Movement"); button(Fire,"Fire","Actions"); button(Use,"Use / Open Door","Actions");
    button(Menu,"Menu / Escape","Menus"); button(Confirm,"Enter / Respawn","Menus");
    button(Map,"Automap","Menus"); button(Yes,"Yes","Menus"); button(No,"No","Menus");
    button(ViewPlus,"View / Zoom +","Menus"); button(ViewMinus,"View / Zoom -","Menus");
    const char* weapons[]={"1 - Fist / Saw","2 - Pistol","3 - Shotgun","4 - Chaingun","5 - Rocket","6 - Plasma","7 - BFG"};
    for (unsigned i=0;i<7;++i) button(Weapon1+i,weapons[i],"Weapons");
    button(Pause,"Pause Game","Game"); button(Fit,"Keep 4:3","Picture"); values_[Fit]=1;
    FindParamInfo(Fit)->defaultFloatVal=1;
    button(Smooth,"Smooth Scaling","Picture");
    SetParamInfo(Brand,"MITH.xyz",FF_TYPE_TEXT,"https://MITH.xyz");
    SetParamGroup(Brand,"MITH.xyz");
}
Doom::~Doom() { engine_.stop(); }
FFResult Doom::InitGL(const FFGLViewportStruct* vp) {
    if (!vp) return FF_FAIL;
    GLState state;
    if (!shader_.Compile(vertex,fragment)) return FF_FAIL;
    glGenTextures(1,&texture_); glGenVertexArrays(1,&vao_); glGenSamplers(1,&sampler_);
    glSamplerParameteri(sampler_,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glSamplerParameteri(sampler_,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glSamplerParameteri(sampler_,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glSamplerParameteri(sampler_,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture_);
    GLint buffer; glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&buffer); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    GLint alignment,row,skipRows,skipPixels,swap;
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment); glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows); glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels); glGetIntegerv(GL_UNPACK_SWAP_BYTES,&swap);
    glPixelStorei(GL_UNPACK_ALIGNMENT,4); glPixelStorei(GL_UNPACK_ROW_LENGTH,0); glPixelStorei(GL_UNPACK_SKIP_ROWS,0); glPixelStorei(GL_UNPACK_SKIP_PIXELS,0); glPixelStorei(GL_UNPACK_SWAP_BYTES,0);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,MD_WIDTH,MD_HEIGHT,0,GL_BGRA,GL_UNSIGNED_BYTE,splash);
    glPixelStorei(GL_UNPACK_ALIGNMENT,alignment); glPixelStorei(GL_UNPACK_ROW_LENGTH,row); glPixelStorei(GL_UNPACK_SKIP_ROWS,skipRows); glPixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels); glPixelStorei(GL_UNPACK_SWAP_BYTES,swap); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,buffer);
    uploaded_=0;
    { std::lock_guard<std::mutex> lock(mutex_); engine_.request(file_); }
    engine_.start();
    return CFFGLPlugin::InitGL(vp);
}
FFResult Doom::DeInitGL() {
    engine_.stop();
    GLState state;
    shader_.FreeGLResources(); glDeleteTextures(1,&texture_); glDeleteVertexArrays(1,&vao_); glDeleteSamplers(1,&sampler_);
    texture_=vao_=sampler_=0; return FF_SUCCESS;
}
FFResult Doom::ProcessOpenGL(ProcessOpenGLStruct* data) {
    if (!data || !shader_.IsReady()) return FF_FAIL;
    bool fit,smooth;
    { std::lock_guard<std::mutex> lock(mutex_);
      uint32_t keys=0; for (unsigned i=0;i<MD_KEYS;++i) if (values_[Forward+i]>.5f) keys|=1u<<i;
      engine_.controls(keys,values_[Pause]>.5f); fit=values_[Fit]>.5f; smooth=values_[Smooth]>.5f; }
    auto frame=engine_.snapshot();
    bool changed=false;
    { std::lock_guard<std::mutex> lock(mutex_); if (!frame.status.empty() && status_!=frame.status) {status_=frame.status;changed=true;} }
    if (changed) RaiseParamEvent(Status,FF_EVENT_FLAG_VALUE);
    GLState state; state.prepare(); glBindFramebuffer(GL_DRAW_FRAMEBUFFER,data->HostFBO);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture_); glBindSampler(0,sampler_);
    if (frame.serial!=uploaded_) {
        GLint buffer,alignment,row,skipRows,skipPixels,swap;
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&buffer); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
        glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment); glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows); glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels); glGetIntegerv(GL_UNPACK_SWAP_BYTES,&swap);
        glPixelStorei(GL_UNPACK_ALIGNMENT,4); glPixelStorei(GL_UNPACK_ROW_LENGTH,0); glPixelStorei(GL_UNPACK_SKIP_ROWS,0); glPixelStorei(GL_UNPACK_SKIP_PIXELS,0); glPixelStorei(GL_UNPACK_SWAP_BYTES,0);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,MD_WIDTH,MD_HEIGHT,GL_BGRA,GL_UNSIGNED_BYTE,
            frame.status.rfind("Playing:",0)==0 ? frame.pixels.data() : splash);
        glPixelStorei(GL_UNPACK_ALIGNMENT,alignment); glPixelStorei(GL_UNPACK_ROW_LENGTH,row); glPixelStorei(GL_UNPACK_SKIP_ROWS,skipRows); glPixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels); glPixelStorei(GL_UNPACK_SWAP_BYTES,swap); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,buffer);
        uploaded_=frame.serial;
    }
    glSamplerParameteri(sampler_,GL_TEXTURE_MIN_FILTER,smooth?GL_LINEAR:GL_NEAREST);
    glSamplerParameteri(sampler_,GL_TEXTURE_MAG_FILTER,smooth?GL_LINEAR:GL_NEAREST);
    glBindVertexArray(vao_); glUseProgram(shader_.GetGLID()); shader_.Set("Image",0);
    shader_.Set("Resolution",float(state.viewport[2]),float(state.viewport[3])); shader_.Set("Fit",fit?1:0);
    glDrawArrays(GL_TRIANGLES,0,3); return FF_SUCCESS;
}
FFResult Doom::SetFloatParameter(unsigned p,float v) {
    if (p>=Count || p==File || p==Status || p==Brand || !std::isfinite(v)) return FF_FAIL;
    std::lock_guard<std::mutex> lock(mutex_);
    if (p==Restart) { if (v>.5f) { for (unsigned i=Forward;i<=ViewMinus;++i) values_[i]=0; engine_.request(file_); } return FF_SUCCESS; }
    values_[p]=std::clamp(v,0.f,1.f); return FF_SUCCESS;
}
float Doom::GetFloatParameter(unsigned p) { std::lock_guard<std::mutex> lock(mutex_); return p<Count?values_[p]:0; }
FFResult Doom::SetTextParameter(unsigned p,const char* v) {
    if (p==Status || p==Brand) return FF_SUCCESS;
    if (p!=File || !v) return FF_FAIL;
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_!=v) { file_=v; for(unsigned i=Forward;i<=ViewMinus;++i) values_[i]=0; engine_.request(file_); }
    return FF_SUCCESS;
}
char* Doom::GetTextParameter(unsigned p) {
    thread_local std::string text;
    std::lock_guard<std::mutex> lock(mutex_); text=p==File?file_:p==Status?status_:p==Brand?"https://MITH.xyz":""; return text.data();
}
char* Doom::GetParameterDisplay(unsigned p) {
    if (p==File || p==Status || p==Brand) return GetTextParameter(p);
    return const_cast<char*>(GetFloatParameter(p)>.5f?"On":"Off");
}
