#pragma once
#ifdef _WIN32
#include <GL/glew.h>
#else
#include <OpenGL/gl3.h>
#endif
// Restore every state modified by allocation, particle accumulation and compositing.
struct GLState {
    GLint program,vao,drawFbo,active,texture,sampler,polygon[2],viewport[4];
    GLint srcRGB,dstRGB,srcAlpha,dstAlpha,eqRGB,eqAlpha;
    GLfloat clear[4]; GLboolean color[4];
    static constexpr GLenum caps[]={GL_BLEND,GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,
        GL_STENCIL_TEST,GL_FRAMEBUFFER_SRGB,GL_RASTERIZER_DISCARD,GL_COLOR_LOGIC_OP,
        GL_SAMPLE_ALPHA_TO_COVERAGE};
    GLboolean enabled[sizeof(caps)/sizeof(*caps)];
    GLState(){
        glGetIntegerv(GL_CURRENT_PROGRAM,&program); glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFbo); glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
        glActiveTexture(GL_TEXTURE0); glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
        glGetIntegerv(GL_SAMPLER_BINDING,&sampler); glActiveTexture(active);
        glGetIntegerv(GL_POLYGON_MODE,polygon); glGetIntegerv(GL_VIEWPORT,viewport);
        glGetBooleanv(GL_COLOR_WRITEMASK,color); glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
        glGetIntegerv(GL_BLEND_SRC_RGB,&srcRGB); glGetIntegerv(GL_BLEND_DST_RGB,&dstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB,&eqRGB); glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eqAlpha);
        for(unsigned i=0;i<sizeof(caps)/sizeof(*caps);++i) enabled[i]=glIsEnabled(caps[i]);
    }
    ~GLState(){
        glUseProgram(program); glBindVertexArray(vao); glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFbo);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture); glBindSampler(0,sampler);
        glActiveTexture(active); glPolygonMode(GL_FRONT_AND_BACK,polygon[0]);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        glColorMask(color[0],color[1],color[2],color[3]); glClearColor(clear[0],clear[1],clear[2],clear[3]);
        glBlendFuncSeparate(srcRGB,dstRGB,srcAlpha,dstAlpha); glBlendEquationSeparate(eqRGB,eqAlpha);
        for(unsigned i=0;i<sizeof(caps)/sizeof(*caps);++i) if(enabled[i]) glEnable(caps[i]); else glDisable(caps[i]);
    }
    void prepare(){
        for(auto cap:caps) glDisable(cap);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    }
};
