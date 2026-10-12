#pragma once
#include <FFGLSDK.h>
#include "Engine.h"
#include <array>
#include <mutex>
class Doom : public CFFGLPlugin {
public:
    enum Param { File, Restart, Status, Forward, Back, Left, Right, StrafeLeft, StrafeRight,
        Fire, Use, Run, Menu, Confirm, Map, Weapon1, Weapon2, Weapon3, Weapon4,
        Weapon5, Weapon6, Weapon7, Yes, No, ViewPlus, ViewMinus, Pause, Fit, Smooth, Brand, Count };
    Doom();
    ~Doom() override;
    FFResult InitGL(const FFGLViewportStruct*) override;
    FFResult DeInitGL() override;
    FFResult ProcessOpenGL(ProcessOpenGLStruct*) override;
    FFResult SetFloatParameter(unsigned, float) override;
    float GetFloatParameter(unsigned) override;
    FFResult SetTextParameter(unsigned, const char*) override;
    char* GetTextParameter(unsigned) override;
    char* GetParameterDisplay(unsigned) override;
private:
    void button(unsigned, const char*, const char*);
    Engine engine_;
    ffglex::FFGLShader shader_;
    GLuint texture_=0, vao_=0, sampler_=0;
    uint64_t uploaded_=0;
    std::mutex mutex_;
    std::array<float, Count> values_{};
    std::string file_, status_="Choose a Doom WAD in Game > WAD File";
};
