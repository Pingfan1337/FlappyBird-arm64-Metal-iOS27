#include "../src/App/MetalGeometry.hpp"
#include <cassert>
#include <iostream>
using namespace flappy12;
int main() {
    for(auto layout:{ViewLayout::make(440,956,62,34),ViewLayout::make(393,852,59,34),ViewLayout::make(320,568),ViewLayout::make(320,480)}) {
        OriginalBird bird;bird.flapCeiling=layout.flightCeiling();
        // The old internal-canvas boundary is no longer an invisible wall.
        bird.y=-1;assert(bird.flap());
        bird.y=bird.flapCeiling;assert(bird.flap());
        assert(bird.y+layout.offsetY+OriginalBird::maxSpriteBottom<=0);
        // Even the entire rotated sprite has left the visible screen before
        // the flap cutoff. GPU clipping must remove it, not clamp its position.
        for(int angle:{-20,0,45,90}) {
            std::vector<Vertex> vertices;
            RenderQuad q{float(bird.spriteX())+layout.offsetX,float(bird.spriteY())+layout.offsetY,48,48,0,0,1,1,1,float(angle)};
            appendQuad(vertices,q,layout.width,layout.height);
            for(const auto& v:vertices)assert(v.position.y>=1);
        }
        // Repeated input above the limit cannot sustain infinite ascent.
        bird.tick();assert(bird.y<bird.flapCeiling);
        int minimum=bird.y,steps=0;
        while(bird.y<bird.flapCeiling&&steps<120) {
            float before=bird.velocity;assert(!bird.flap());assert(bird.velocity==before);
            bird.tick();minimum=std::min(minimum,bird.y);++steps;
        }
        assert(steps<120&&bird.y>=bird.flapCeiling);
        assert(minimum>=bird.flapCeiling-OriginalBird::upperFlightMargin);
        assert(bird.flap()); // Naturally falling back below the limit re-enables input.
        bird.reset();assert(bird.flapCeiling==layout.flightCeiling());
        bird.hit=true;assert(!bird.flap());
        // There must be no bypass above upper pipes after moving the ceiling.
        bird.x=150;bird.y=minimum;
        for(int lowerTop:{180,274,359}) {
            assert(bird.overlapsPair(150,lowerTop));
            assert(!bird.overlapsPair(203,lowerTop));
            bird.y=lowerTop-96+1;assert(!bird.overlapsPair(150,lowerTop));
            bird.y=minimum;
        }
        // Highest opening: a short shaft must still reach the visible top.
        OriginalGame game;game.atlas["bg_day"]={0,0,288,512};game.atlas["land"]={584,0,336,112};
        game.atlas["pipe_down"]={112,646,52,320};game.draws={{"pipe_down",150,-57,0,1}};
        std::vector<RenderQuad> quads;composeFrame(game,{},1,layout,quads);
        if(layout.offsetY-57>-8){assert(quads.size()==4);assert(quads[2].y==-8);assert(std::abs(quads[2].y+quads[2].h-quads[3].y)<1e-5);}
    }
    OriginalBird original;original.y=-1;assert(!original.flap()); // Legacy policy is preserved.
    std::cout<<"Flight ceiling checks passed: offscreen flight, finite ascent, natural recovery, reset policy, upper-pipe collision and shaft coverage\n";
}
