#pragma once
#include "../Core/Presentation.hpp"

#include <vector>
struct alignas(8) VertexPoint {float x,y;};
struct Vertex {VertexPoint position,uv;float alpha;};
static_assert(sizeof(Vertex)==24,"Metal vertex ABI");
inline void appendQuad(std::vector<Vertex>& vertices,const flappy12::RenderQuad& r,float width,float height) {
    float xs[]={r.x,r.x+r.w,r.x+r.w,r.x},ys[]={r.y+r.h,r.y+r.h,r.y,r.y};
    float us[]={r.u,r.u+r.uw,r.u+r.uw,r.u},vs[]={r.v+r.vh,r.v+r.vh,r.v,r.v};
    Vertex q[4];float cx=r.x+r.w/2,cy=r.y+r.h/2;
    float radians=r.angle*static_cast<float>(3.1415926/180.0);
    float sn=std::sin(radians),cs=std::cos(radians);
    for(int i=0;i<4;++i) {
        float px=xs[i],py=ys[i];
        if(r.angle!=0){float dx=px-cx,dy=py-cy;px=dx*cs-dy*sn+cx;py=dx*sn+dy*cs+cy;}
        q[i]={{px/width*2-1,1-py/height*2},{us[i],vs[i]},r.alpha};
    }
    for(int i:{0,1,2,0,2,3})vertices.push_back(q[i]);
}
