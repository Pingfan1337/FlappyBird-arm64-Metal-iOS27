#pragma once
#include "OriginalGame.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace flappy12 {
// Fixed-step gameplay is intentionally independent of display refresh rate.
class PresentationClock {
public:
    static constexpr double step = 1.0 / 60.0;
    double last = 0, remainder = 0;
    bool primed = false;
    void reset() { last=0; remainder=0; primed=false; }
    int advance(double timestamp) {
        if(!primed){primed=true;last=timestamp;return 1;}
        // Bound catch-up after system pauses; avoid a long main-thread stall.
        remainder += clampValue(timestamp-last,0.0,0.1);
        last=timestamp;
        int ticks=static_cast<int>((remainder+1e-9)/step);
        remainder=std::max(0.0,remainder-ticks*step);
        return ticks;
    }
    float blend() const {return static_cast<float>(clampValue(remainder/step,0.0,1.0));}
};
struct ViewLayout {
    float width=288,height=512,offsetX=0,offsetY=0,scale=1;
    static ViewLayout make(float pointsWidth,float pointsHeight,float safeTop=0,float safeBottom=0) {
        ViewLayout l;
        if(pointsWidth<=0||pointsHeight<=0)return l;
        l.scale=std::min(pointsWidth/288.f,pointsHeight/512.f);
        l.width=pointsWidth/l.scale;l.height=pointsHeight/l.scale;
        l.offsetX=(l.width-288)/2;
        l.offsetY=clampValue((l.height-512+(safeTop-safeBottom)/l.scale)/2,0.f,std::max(0.f,l.height-512));
        return l;
    }
    int inputX(float x) const {return static_cast<int>(x/scale-offsetX);}
    int inputY(float y) const {return static_cast<int>(y/scale-offsetY);}
    int flightCeiling() const {
        return -static_cast<int>(std::ceil(offsetY))-OriginalBird::maxSpriteBottom;
    }
};
struct RenderQuad {
    float x,y,w,h,u,v,uw,vh,alpha,angle;
};
// Match repeated sprites by occurrence, and reject discontinuities (new scenes,
// pipe recycling, a reset bird, or a changed number of repeated sprites).
inline const Draw* previousDraw(const std::vector<Draw>& old,const std::vector<Draw>& now,size_t index) {
    if(now[index].entity!=0) {
        for(const auto& d:old)if(d.entity==now[index].entity)return &d;
        return nullptr;
    }
    const auto& name=now[index].name;
    auto matches=[&](const std::string& candidate){
        // A wing-animation frame is still the same moving bird. Matching only
        // the texture name would briefly break interpolation at each flap.
        return candidate==name||(name.rfind("bird",0)==0&&candidate.rfind("bird",0)==0);
    };
    int occurrence=0,oldCount=0,nowCount=0;
    for(size_t i=0;i<now.size();++i)if(matches(now[i].name)){++nowCount;if(i<index)++occurrence;}
    for(const auto& d:old)if(matches(d.name))++oldCount;
    if(oldCount!=nowCount)return nullptr;
    for(const auto& d:old)if(matches(d.name)&&occurrence--==0)return &d;
    return nullptr;
}
inline void composeFrame(const OriginalGame& game,const std::vector<Draw>& old,float blend,const ViewLayout& layout,std::vector<RenderQuad>& out) {
    out.clear();
    auto add=[&](Sprite s,float x,float y,float w,float h,float alpha=1,float angle=0){
        out.push_back({x,y,w,h,s.x/1024.f,s.y/1024.f,s.w/1024.f,s.h/1024.f,alpha,angle});
    };
    // Overscan avoids exposing black at viewport edges during the original shake.
    Sprite bg=game.sprite(game.night?"bg_night":"bg_day");
    Sprite sky{bg.x+1,bg.y+1,1,1};
    Sprite land=game.sprite("land");Sprite sand{land.x+40,land.y+40,1,1};
    add(sky,-8,-8,layout.width+16,layout.height+16);
    out.back().u+=.5f/1024;out.back().v+=.5f/1024;out.back().uw=out.back().vh=0;
    float floor=layout.offsetY+400;
    add(sand,-8,floor,layout.width+16,layout.height-floor+8);
    out.back().u+=.5f/1024;out.back().v+=.5f/1024;out.back().uw=out.back().vh=0;
    for(size_t i=0;i<game.draws.size();++i) {
        const auto& d=game.draws[i];auto s=game.sprite(d.name);
        float x=d.x,y=d.y,angle=d.angle,alpha=d.alpha;
        if(const auto* p=previousDraw(old,game.draws,i)) {
            float targetX=d.x;
            if(d.name=="land"&&targetX>p->x)targetX-=24;
            if(std::abs(targetX-p->x)<=32&&std::abs(d.y-p->y)<=32) {
                x=p->x+(targetX-p->x)*blend;y=p->y+(d.y-p->y)*blend;
                angle=p->angle+(d.angle-p->angle)*blend;
                alpha=p->alpha+(d.alpha-p->alpha)*blend;
            }
        }
        if(d.name=="black"||d.name=="white")add(s,-8,-8,layout.width+16,layout.height+16,alpha);
        else {
            // The visible sky can extend above the original 320px pipe shaft.
            // Extend only its shaft, keeping the opening and bottom cap fixed.
            if(d.name=="pipe_down"&&y+layout.offsetY>-8) {
                Sprite shaft=s;shaft.h=32;
                add(shaft,x+layout.offsetX,-8,s.w,y+layout.offsetY+8,alpha);
            }
            // Keep the original foreground ordering: the ground must cover
            // pipes already drawn behind it, all the way to the screen bottom.
            if(d.name=="bg_day"||d.name=="bg_night")s.h=400;
            if(d.name=="land") {
                float groundY=y+layout.offsetY;
                add(sand,-8,groundY,layout.width+16,layout.height-groundY+8,alpha);
                out.back().u+=.5f/1024;out.back().v+=.5f/1024;
                out.back().uw=out.back().vh=0;
                // Only the textured top strip is needed over the solid fill.
                s.h=32;
            }
            add(s,x+layout.offsetX,y+layout.offsetY,s.w,s.h,alpha,angle);
        }
    }
    for(auto& q:out){q.x+=game.shakeX;q.y+=game.shakeY;}
}
} // namespace flappy12
