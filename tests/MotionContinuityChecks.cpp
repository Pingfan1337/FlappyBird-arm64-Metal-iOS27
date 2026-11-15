#include "../src/Core/Presentation.hpp"
#include "../src/Core/FramePacingStats.hpp"
#include <cassert>
#include <iostream>
#include <map>
using namespace flappy12;
void trace(bool variableRefresh) {
    OriginalGame game;game.atlas["pipe_up"]={168,646,52,320};
    std::array<int,3> positions{{0,157,314}};
    std::array<uint64_t,3> identities{{1,2,3}};uint64_t next=4;
    auto snapshot=[&]{game.draws.clear();for(int i=0;i<3;++i)game.draws.push_back({"pipe_up",positions[i],274,0,1,identities[i]});};
    snapshot();std::vector<Draw> previous;std::vector<RenderQuad> quads;
    PresentationClock clock;double time=10;int totalTicks=0;double worst=0;
    std::map<uint64_t,std::pair<double,double>> observed;
    for(int frame=0;frame<4000;++frame) {
        if(frame)time+=variableRefresh&&frame%3==0?1./80:1./120;
        int ticks=clock.advance(time);
        for(int t=0;t<ticks;++t) {
            previous=game.draws;for(auto& x:positions)x-=2;
            if(positions[0]<-52){positions[0]=positions[1];positions[1]=positions[2];positions[2]+=157;identities[0]=identities[1];identities[1]=identities[2];identities[2]=next++;}
            snapshot();++totalTicks;
        }
        composeFrame(game,previous,clock.blend(),ViewLayout{},quads);
        for(int i=0;i<3;++i) {
            double x=quads[2+i].x;
            if(x<=288&&x>=-52) {
                double expected=(identities[i]-1)*157.0-2*(totalTicks-1+clock.blend());
                worst=std::max(worst,std::abs(x-expected));assert(std::abs(x-expected)<.001);
                auto it=observed.find(identities[i]);
                if(it!=observed.end()){double movement=x-it->second.second;double expectedMovement=-120*(time-it->second.first);assert(std::abs(movement-expectedMovement)<.001);}
                observed[identities[i]]={time,x};
            }
        }
    }
    std::cout<<(variableRefresh?"Variable refresh":"120Hz")<<" pipe trace: 4000 frames, maximum position error "<<worst<<" logical pixel\n";
}
int main() {
    trace(false);trace(true);
    FramePacingStats stats;for(int i=0;i<121;++i)stats.presented(10+i/120.0);
    auto first=stats.take();assert(first.frames==121&&std::abs(first.p99Milliseconds-1000./120)<1e-6);
    assert(stats.take().frames==0);
    for(int i=1;i<=5;++i)stats.presented(11+i/30.0);
    assert(stats.take().p99Milliseconds>30);
    stats.reset();stats.presented(0);assert(stats.take().frames==0);
    stats.presented(50);stats.presented(49);assert(stats.take().frames==1);
    std::cout<<"Frame pacing telemetry checks passed\n";
}
