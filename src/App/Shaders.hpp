#pragma once
static const char* gameShaderSource = R"(
#include <metal_stdlib>
using namespace metal;
struct V { float2 p; float2 uv; float a; };
struct O { float4 p [[position]]; float2 uv; float a; };
vertex O sprite_vertex(uint i [[vertex_id]],const device V* v [[buffer(0)]]) {
 O o; o.p=float4(v[i].p,0,1);o.uv=v[i].uv;o.a=v[i].a;return o;
}
fragment float4 sprite_fragment(O o [[stage_in]],texture2d<float> t [[texture(0)]],sampler s [[sampler(0)]]) {
 return t.sample(s,o.uv)*o.a;
}
)";
