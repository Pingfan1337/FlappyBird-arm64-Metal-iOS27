#pragma once
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#include "MetalGeometry.hpp"
#include "AtlasPixels.hpp"
#include "GLShaders.hpp"
class GLRenderer {
public:
    EAGLContext* context=nil;
    GLuint framebuffer=0,renderbuffer=0,texture=0,program=0,vbo=0;
    GLint width=0,height=0;
    bool initialize(CAEAGLLayer* layer,const AtlasPixels& atlas) {
        layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
        context=[[EAGLContext alloc]initWithAPI:kEAGLRenderingAPIOpenGLES2];if(!context)return false;
        [EAGLContext setCurrentContext:context];
        glGenFramebuffers(1,&framebuffer);glGenRenderbuffers(1,&renderbuffer);resize(layer);
        GLuint shaders[2];const GLenum types[2]={GL_VERTEX_SHADER,GL_FRAGMENT_SHADER};const char* sources[2]={gameGLVertex,gameGLFragment};
        program=glCreateProgram();
        for(int i=0;i<2;++i){shaders[i]=glCreateShader(types[i]);glShaderSource(shaders[i],1,&sources[i],NULL);glCompileShader(shaders[i]);GLint ok=0;glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&ok);if(!ok)return false;glAttachShader(program,shaders[i]);}
        glBindAttribLocation(program,0,"position");glBindAttribLocation(program,1,"texcoord");glBindAttribLocation(program,2,"alpha");glLinkProgram(program);
        GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);for(int i=0;i<2;++i)glDeleteShader(shaders[i]);if(!ok)return false;
        glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,(GLsizei)atlas.width,(GLsizei)atlas.height,0,GL_RGBA,GL_UNSIGNED_BYTE,atlas.data.bytes);
        glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,4096*sizeof(Vertex),NULL,GL_STREAM_DRAW);
        return true;
    }
    void resize(CAEAGLLayer* layer) {
        if(!context)return;[EAGLContext setCurrentContext:context];glBindRenderbuffer(GL_RENDERBUFFER,renderbuffer);[context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_WIDTH,&width);glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_HEIGHT,&height);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,renderbuffer);
    }
    void draw(const std::vector<Vertex>& vertices) {
        if(!context||width<=0||height<=0)return;[EAGLContext setCurrentContext:context];glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glViewport(0,0,width,height);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glUseProgram(program);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glUniform1i(glGetUniformLocation(program,"atlas"),0);
        glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferSubData(GL_ARRAY_BUFFER,0,vertices.size()*sizeof(Vertex),vertices.data());
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(const void*)0);glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(const void*)8);glEnableVertexAttribArray(2);glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,sizeof(Vertex),(const void*)16);
        glDrawArrays(GL_TRIANGLES,0,(GLsizei)vertices.size());glBindRenderbuffer(GL_RENDERBUFFER,renderbuffer);[context presentRenderbuffer:GL_RENDERBUFFER];
    }
};
