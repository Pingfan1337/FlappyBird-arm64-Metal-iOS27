#pragma once
static const char* gameGLVertex=R"(
#ifdef GL_ES
precision highp float;
#define UVP mediump
#define AP lowp
#else
#define UVP
#define AP
#endif
attribute vec2 position;
attribute vec2 texcoord;
attribute float alpha;
varying UVP vec2 uv;
varying AP float a;
void main(){gl_Position=vec4(position,0.0,1.0);uv=texcoord;a=alpha;}
)";
static const char* gameGLFragment=R"(
#ifdef GL_ES
precision mediump float;
#define UVP mediump
#define AP lowp
#else
#define UVP
#define AP
#endif
varying UVP vec2 uv;
varying AP float a;
uniform sampler2D atlas;
void main(){gl_FragColor=texture2D(atlas,uv)*a;}
)";
