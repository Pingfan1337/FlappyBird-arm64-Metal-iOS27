#include "../src/Core/OriginalGame.hpp"
#include <fstream>
#include <cassert>
#include <iostream>
using namespace flappy12;
int main(){OriginalGame g;std::ifstream f("resources/atlas.txt");std::string n;int w,h;float a,b,c,d;while(f>>n>>w>>h>>a>>b>>c>>d)g.atlas[n]={int(a*1024+.5),int(b*1024+.5),w,h};assert(g.atlas.size()==71);g.seed(1234);g.initialize(0);for(int i=0;i<61;++i)g.frame();assert(g.menu);g.handle(5);for(int i=0;i<31;++i)g.frame();assert(!g.menu&&g.readyMode==1);bool hasScore=false;for(auto& d:g.draws)if(d.name=="font_048")hasScore=true;assert(hasScore);g.touchBegin(150,250);assert(!g.bird.hovering);for(int i=0;i<300;++i)g.frame();assert(g.speed==0);assert(g.panelMode==2);std::cout<<"Game state checks passed\n";}
