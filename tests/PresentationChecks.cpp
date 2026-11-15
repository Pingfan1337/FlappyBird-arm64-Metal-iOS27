#include "../src/Core/Presentation.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <tuple>
using namespace flappy12;
using State=std::tuple<int,int,float,int,std::array<int,3>,std::array<int,3>,uint32_t,uint32_t>;
State simulate(int hz) {
    std::srand(1);OriginalGame game;game.seed(1234);
    std::ifstream f("resources/atlas.txt");
    std::string n;int w,h;float a,b,c,d;
    while(f>>n>>w>>h>>a>>b>>c>>d)game.atlas[n]={int(a*1024+.5),int(b*1024+.5),w,h};
    game.initialize(0);game.handle(5);
    PresentationClock clock;int total=0;
    for(int callback=0;callback<=hz*4;++callback) {
        int ticks=clock.advance(10.0+double(callback)/hz);
        for(int i=0;i<ticks;++i){if(total==40||total==70)game.flap();game.frame();++total;}
    }
    assert(total==241);
    return {game.bird.y,game.bird.x,game.bird.velocity,game.score,game.pipeX,game.pipeY,game.randomW,game.randomZ};
}
int main() {
    assert(simulate(30)==simulate(60));assert(simulate(60)==simulate(120));assert(simulate(120)==simulate(80));
    PresentationClock c;assert(c.advance(1)==1);assert(c.advance(1+1./120)==0);assert(std::abs(c.blend()-.5f)<1e-5);assert(c.advance(1+1./60)==1);
    c.reset();assert(c.advance(100)==1);assert(c.blend()==0);
    assert(c.advance(101)<=6); // Foreground scheduling stall: bounded catch-up.
    auto layout=ViewLayout::make(440,956,62,34);
    assert(layout.width==288&&layout.height>512&&layout.offsetY>0);
    assert(std::abs(layout.inputX((80+layout.offsetX)*layout.scale)-80)<=1);
    assert(std::abs(layout.inputY((246+layout.offsetY)*layout.scale)-246)<=1);
    assert(ViewLayout::make(320,480).offsetY==0);
    OriginalGame g;g.atlas["bg_day"]={0,0,288,512};g.atlas["land"]={584,0,336,112};g.atlas["bird0_0"]={0,970,48,48};
    g.draws={{"bird0_0",80,100,-20,1},{"land",0,400,0,1}};
    std::vector<Draw> old={{"bird0_0",80,104,-10,1},{"land",-22,400,0,1}};
    std::vector<RenderQuad> q;composeFrame(g,old,.5,layout,q);
    assert(q.size()==5);
    assert(std::abs(q[2].y-layout.offsetY-102)<1e-4&&q[2].angle==-15);
    assert(q[4].x==-23); // Scroll wraps smoothly at the 24-pixel texture repeat.
    assert(q[0].x<=0&&q[0].y<=0&&q[0].w>=layout.width&&q[0].h>=layout.height);
    assert(q[1].y<=layout.offsetY+400&&q[1].y+q[1].h>=layout.height);
    old[0].name="bird0_1";composeFrame(g,old,.5,layout,q);
    assert(std::abs(q[2].y-layout.offsetY-102)<1e-4); // Still smooth when wings change.
    old[0].x=200;composeFrame(g,old,.5,layout,q);assert(q[2].x==80); // Don't smear a reset.
    // A lower pipe extends beyond y=400. A foreground, screen-wide ground fill
    // must follow the pipe, then the moving top strip; the back fill cannot hide it.
    g.atlas["pipe_up"]={112,646,52,320};
    g.draws={{"pipe_up",150,300,0,1},{"land",-2,400,0,1}};
    for(auto l:{ViewLayout::make(440,956,62,34),ViewLayout::make(320,568),ViewLayout::make(320,480)}) {
        composeFrame(g,{},1,l,q);
        assert(q.size()==5);
        const auto& pipe=q[2];const auto& mask=q[3];const auto& strip=q[4];
        assert(pipe.y+pipe.h>mask.y);
        assert(mask.y==l.offsetY+400&&mask.y+mask.h>=l.height);
        assert(mask.x<=0&&mask.x+mask.w>=l.width);
        assert(mask.uw==0&&mask.vh==0&&mask.alpha==1);
        assert(strip.y==mask.y&&strip.h==32);
    }
    std::cout<<"Presentation checks passed: refresh-rate-independent gameplay, interpolation, wrapping, full-screen coverage and touch mapping\n";
}
