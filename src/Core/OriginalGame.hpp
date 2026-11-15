#pragma once
#include "OriginalBird.hpp"
#include <array>
#include <vector>
#include <string>
#include <map>
#include <cstdlib>
#include <cstring>

namespace flappy12 {
struct Sprite { int x=0,y=0,w=0,h=0; };
struct Draw { std::string name; int x,y,angle; float alpha; uint64_t entity=0; };
struct Tween {
    int frame=0,frames=1,mode=0; float from=0,to=0,value=0; bool done=true;
    void start(float a,float b,int n,int m=0) {from=a;to=b;frames=n;mode=m;frame=0;value=a;done=false;}
    void tick() {
        if(done) return;
        ++frame;
        if(frame==frames) {done=true;value=to;return;}
        float progress=(1.0f/frames)*frame;
        double t=static_cast<int>(progress*100.0f)/100.0;
        float eased=progress;
        if(mode==5) eased=static_cast<float>((t-1)*((t-1)*(t-1))+1);
        if(mode==11) eased=static_cast<float>((t-1)*((t-1)*((t-1)*((t-1)*(t-1))))+1);
        value=from+eased*(to-from);
    }
};
struct Button {
    std::string name; int x=0,y=0,w=0,h=0; bool shown=false,held=false,released=false;
    bool contains(int px,int py) const {return px>x&&px<x+w&&py>y&&py<y+h;}
    void show(int a,int b) {x=a;y=b;shown=true;held=false;released=false;}
    void update(bool touching,int px,int py) {bool down=touching&&contains(px,py);released=held&&!down;held=down;}
};
// Game state and original integer-frame rules. Rendering and OS services are
// injected by the native shell. External events retain the original event IDs.
class OriginalGame {
public:
    OriginalBird bird;
    std::map<std::string,Sprite> atlas;
    std::vector<Draw> draws;
    std::vector<int> events;
    std::array<Button,8> buttons;
    bool menu=true,night=false,touching=false;
    int touchX=-999,touchY=-999,score=0,best=0,colour=0;
    std::array<int,3> pipeX{{79,236,393}},pipeY{{274,274,274}};
    std::array<uint64_t,3> pipeEntity{{1,2,3}};
    uint64_t nextPipeEntity=4;
    int warmup=1,speed=2,landX=0;
    uint32_t randomW=1,randomZ=1;
    int animationFrame=0,animationClock=0; bool autoAnimation=true,animationDone=false;
    int readyMode=-1; Tween readyAlpha;
    int overMode=-1,overY=-1; float overVelocity=-2; Tween overAlpha;
    int panelMode=-1,panelY=504,panelScore=0,panelBest=0,medal=-1; bool isNew=false; Tween panelTween;
    Tween fade,flash; int fadeEvent=0;
    int shakeFrames=0,shakeX=0,shakeY=0;
    int externalBest=0;
    std::array<int,50> timerTicks{},timerEvent{}; int timerCursor=0;

    OriginalGame() {
        const char* names[]={"button_play","button_score","button_ok","button_menu","button_pause","button_resume","button_share","button_rate"};
        for(int i=0;i<8;++i) buttons[i].name=names[i];
    }
    void seed(uint32_t time) {randomW=time%32000;randomZ=time%65535;}
    int random() {
        randomZ=36969*(randomZ&65535)+(randomZ>>16);
        randomW=18000*(randomW&65535)+(randomW>>16);
        uint32_t u=randomW+(randomZ<<16);int32_t s;std::memcpy(&s,&u,4);
        if(s<0) {u=0u-u;std::memcpy(&s,&u,4);} return s;
    }
    Sprite sprite(const std::string& n) const {auto it=atlas.find(n);return it==atlas.end()?Sprite{}:it->second;}
    void draw(const std::string& n,int x,int y,float alpha=1,int angle=0,uint64_t entity=0) {
        if(sprite(n).w>0&&alpha>0) draws.push_back({n,x,y,angle,alpha,entity});
    }
    void centre(const std::string& n,int y,float a=1) {draw(n,(288-sprite(n).w)/2,y,a);}
    void timer(int id,int delay) {timerEvent[timerCursor]=id;timerTicks[timerCursor]=delay;timerCursor=(timerCursor+1)%50;}
    void resetAnimation(bool automatic) {autoAnimation=automatic;animationFrame=0;animationClock=0;animationDone=false;}
    void resetBird() {bird.reset();colour=(std::rand()%10);colour=colour<=5?0:(colour<=7?1:2);resetAnimation(true);}
    void resetMenu() {
        night=std::rand()%10>5; for(auto& b:buttons)b.shown=false;
        buttons[0].show((272-buttons[0].w-buttons[1].w)/2,340);
        buttons[1].show(buttons[0].x+buttons[0].w+16,340);
        buttons[7].show((288-buttons[7].w)/2,270);
        readyMode=-1;overMode=-1;panelMode=-1;
        menu=true;landX=0;resetBird();speed=2;warmup=1;score=0;
    }
    void initialize(int high) {
        best=high; for(auto& b:buttons){auto s=sprite(b.name);b.w=s.w;b.h=s.h;}
        resetMenu();colour=0;fade.start(1,0,30,5);fade.tick();
    }
    void beginFade(int event) {if(fade.done){fadeEvent=event;fade.start(0,1,30,5);fade.tick();}}
    void handle(int id) {
        if(id==6) {resetMenu();colour=0;fade.start(1,0,30,5);fade.tick();}
        if(id==5) {
            resetMenu();buttons[0].shown=buttons[1].shown=buttons[7].shown=false;
            menu=false;score=0;resetBird();speed=2;warmup=1;
            readyMode=0;readyAlpha.start(0,1,30);events.push_back(7);
            fade.start(1,0,30,5);fade.tick();
        }
        if(id==2) {externalBest=score;events.push_back(0);overMode=0;overAlpha.start(0,1,60,11);overY=-1;overVelocity=-2;events.push_back(10);}
        if(id==3) events.push_back(11);
        if(id==4) events.push_back(13);
    }
    void flap() {if(bird.flap()){resetAnimation(false);timer(4,5);}}
    void touchBegin(int /*x*/,int /*y*/) {
        if(menu||!fade.done) return;
        if(bird.hovering) {
            if(readyMode==1){readyMode=2;readyAlpha.start(1,0,30);flap();}
        } else if(speed>0) flap();
    }
    void die(bool pipe) {
        if(flash.done){flash.start(1,0,60,11);flash.tick();}shakeFrames=30;speed=0;events.push_back(12);
        if(pipe){bird.hit=true;timer(3,500);} timer(2,1000);
    }
    void drawScore(int number) {
        auto str=std::to_string(number);int width=2;
        for(char ch:str) width+=sprite("font_0"+std::to_string(static_cast<int>(ch))).w-4;
        int x=144-width/2;
        for(char ch:str){std::string n="font_0"+std::to_string(static_cast<int>(ch));draw(n,x,78);x+=sprite(n).w-4;}
    }
    void smallNumber(int number,int right,int y) {
        do {int d=number%10;number/=10;draw("number_score_0"+std::to_string(d),right-16,y);right-=16;}while(number>0);
    }
    void frame() {
        draws.clear();events.clear();
        for(int i=0;i<50;++i) if(timerTicks[i]>0){timerTicks[i]-=30;if(timerTicks[i]<=0)handle(timerEvent[i]);}
        draw(night?"bg_night":"bg_day",0,0);
        landX-=speed;if(landX<-23)landX=0;
        if(!bird.hovering) {
            for(auto& x:pipeX)x-=speed;
            if(speed>0&&warmup<=0&&(pipeX[0]==bird.x||pipeX[0]==bird.x-1)){++score;events.push_back(9);}
            if(pipeX[0]<-52){
                pipeEntity[0]=pipeEntity[1];pipeEntity[1]=pipeEntity[2];pipeEntity[2]=nextPipeEntity++;
                pipeX[0]=pipeX[1];pipeY[0]=pipeY[1];pipeX[1]=pipeX[2];pipeY[1]=pipeY[2];pipeX[2]+=157;pipeY[2]=random()%180+180;
                if(warmup>0&&--warmup==0)pipeX[0]=pipeX[1]=-52;
            }
        }
        if(!animationDone){animationClock+=15;int period=autoAnimation?100:33;
            if(animationClock>=period){animationClock=0;++animationFrame;if(animationFrame>=12){if(autoAnimation)animationFrame=0;else animationDone=true;}}
        }
        bird.tick();
        if(menu) {
            centre("title",150);
            bird.x=(288-20)/2;bird.y=sprite("title").h+170;
        } else {
            if(bird.y>=380&&speed>0)die(false);
            if(!bird.hit&&warmup<=0&&speed>0){
                for(int i=0;i<2;++i)if(bird.overlapsPair(pipeX[i],pipeY[i])){die(true);break;}
            }
            // Keep offscreen instances in the render snapshot. Stable identity
            // survives queue rotation and entry into the visible viewport.
            if(warmup<=0)for(int i=0;i<3;++i){draw("pipe_up",pipeX[i],pipeY[i],1,0,pipeEntity[i]*2);draw("pipe_down",pipeX[i],pipeY[i]-416,1,0,pipeEntity[i]*2+1);}
            if(panelMode==2&&!buttons[0].shown){buttons[0].show((272-buttons[0].w-buttons[1].w)/2,340);buttons[1].show(buttons[0].x+buttons[0].w+16,340);}
            if(overMode>=0){
                overAlpha.tick();if(overY<=-1){overY=static_cast<int>(overY+overVelocity);overVelocity+=0.25f;}else overY=0;
                if(overMode==0&&overAlpha.done){overMode=1;showPanel();events.push_back(10);}else if(overMode==1&&panelMode==2)overMode=2;
                centre("text_game_over",overY+130,overAlpha.value);
            }else drawScore(score);
        }
        static const int sequence[]={0,1,2,1,0,1,2,1,0,1,2,1};
        int wing=animationDone?1:sequence[animationFrame];
        draw("bird"+std::to_string(colour)+"_"+std::to_string(wing),bird.spriteX(),bird.spriteY(),1,bird.spriteAngle(),UINT64_MAX);
        draw("land",landX,400,1,0,UINT64_MAX-1);
        if(readyMode>=0){readyAlpha.tick();if(readyAlpha.done){if(readyMode==0)readyMode=1;else if(readyMode==2)readyMode=-1;}
            if(readyMode>=0){centre("text_ready",146,readyAlpha.value);centre("tutorial",220,readyAlpha.value);}}
        if(menu)centre("brand_copyright",432-sprite("brand_copyright").h);
        for(auto& b:buttons)if(b.shown){b.update(touching,touchX,touchY);draw(b.name,b.x,b.y+(b.held?2:0));}
        if(buttons[0].shown&&buttons[0].released){beginFade(5);events.push_back(10);}
        if(buttons[1].shown&&buttons[1].released){events.push_back(1);events.push_back(10);}
        if(buttons[7].shown&&buttons[7].released)events.push_back(2);
        updatePanel();
        if(!fade.done||fade.value!=0){fade.tick();if(fade.done&&fade.value==1)handle(fadeEvent);}
        if(shakeFrames>0){--shakeFrames;shakeX=random()%8-4;shakeY=random()%8-4;}else shakeX=shakeY=0;
        if(!flash.done)flash.tick();
        if(fade.value>0)draw("black",0,0,fade.value);
        if(flash.value>0)draw("white",0,0,flash.value);
    }
    void showPanel() {panelMode=0;panelY=504;panelScore=0;panelBest=best;medal=-1;isNew=false;panelTween.start(504,193,30,11);}
    void updatePanel() {
        if(panelMode<0)return;
        panelTween.tick();
        if(panelMode==0){panelY=static_cast<int>(panelTween.value);if(panelTween.done){if(score<1)panelMode=2;else{panelMode=1;panelTween.start(0,static_cast<float>(score),30);}}}
        else if(panelMode==1){panelScore=static_cast<int>(panelTween.value);if(panelTween.done){panelMode=2;if(panelScore>panelBest){panelBest=panelScore;best=panelScore;isNew=true;}medal=score>=40?0:score>=30?1:score>=20?2:score>=10?3:-1;}}
        int x=25;draw("score_panel",x,panelY);smallNumber(panelScore,x+210,panelY+36);smallNumber(panelBest,x+210,panelY+78);
        if(isNew)draw("new",x+142,panelY+60);
        if(medal>=0)draw("medals_"+std::to_string(medal),x+32,panelY+44);
    }
};
} // namespace flappy12
