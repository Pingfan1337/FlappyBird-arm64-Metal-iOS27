#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>
#import <AVFoundation/AVFoundation.h>
#include "MetalGeometry.hpp"
#include "../Core/FramePacingStats.hpp"
#include "AudioBackend.hpp"
#import <QuartzCore/QuartzCore.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <simd/simd.h>

using namespace flappy12;
#include "Shaders.hpp"
@class GameController;
@interface GameView : UIView
@property(nonatomic,weak) GameController* owner;
@end
@interface GameController : UIViewController<CAMetalDisplayLinkDelegate> {
@public
    OriginalGame game;
    id<MTLCommandQueue> queue;
    id<MTLRenderPipelineState> pipeline;
    id<MTLTexture> atlasTexture;
    id<MTLSamplerState> linearSampler;
    id<MTLBuffer> frameBuffers[3];
    dispatch_semaphore_t inFlight;
    NSUInteger frameIndex;
    std::vector<Draw> previousDraws;
    std::vector<RenderQuad> renderQuads;
    std::vector<Vertex> vertices;
    PresentationClock clock;
    ViewLayout layout;
    FramePacingStats pacing;
    CGSize pendingDrawableSize;
    CFTimeInterval statsStart;
    GameAudio* audio;
}
@property(nonatomic,strong) GameView* gameView;
@property(nonatomic,strong) CAMetalDisplayLink* displayLink;
@property(nonatomic,strong) UILabel* fpsLabel;
-(void)renderDrawable:(id<CAMetalDrawable>)drawable;
-(void)touchBegin:(CGPoint)p;
-(void)touchUpdate:(CGPoint)p down:(BOOL)down;
@end
@implementation GameView
+(Class)layerClass {return CAMetalLayer.class;}
-(void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
 UITouch* t=touches.anyObject;CGPoint p=[t locationInView:self];[self.owner touchUpdate:p down:YES];[self.owner touchBegin:p];
}
-(void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {[self.owner touchUpdate:[touches.anyObject locationInView:self] down:YES];}
-(void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {[self.owner touchUpdate:[touches.anyObject locationInView:self] down:NO];}
-(void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {[self.owner touchUpdate:CGPointZero down:NO];}
@end

@implementation GameController
-(void)loadView {
 self.view=[[UIView alloc]initWithFrame:UIScreen.mainScreen.bounds];self.view.backgroundColor=UIColor.blackColor;
 id<MTLDevice> device=MTLCreateSystemDefaultDevice();
 if(!device) @throw [NSException exceptionWithName:@"MetalUnavailable" reason:@"Metal device unavailable" userInfo:nil];
 self.gameView=[[GameView alloc]initWithFrame:CGRectZero];self.gameView.owner=self;
 CAMetalLayer* metalLayer=(CAMetalLayer*)self.gameView.layer;
 metalLayer.device=device;metalLayer.pixelFormat=MTLPixelFormatBGRA8Unorm;
 metalLayer.framebufferOnly=YES;metalLayer.maximumDrawableCount=3;metalLayer.presentsWithTransaction=NO;metalLayer.opaque=YES;
 [self.view addSubview:self.gameView];
 queue=[device newCommandQueue];NSError* error=nil;
 id<MTLLibrary> library=[device newLibraryWithSource:@(gameShaderSource) options:nil error:&error];
 if(!library) @throw [NSException exceptionWithName:@"ShaderError" reason:error.localizedDescription userInfo:nil];
 MTLRenderPipelineDescriptor* pd=[MTLRenderPipelineDescriptor new];
 pd.vertexFunction=[library newFunctionWithName:@"sprite_vertex"];pd.fragmentFunction=[library newFunctionWithName:@"sprite_fragment"];
 pd.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm;pd.colorAttachments[0].blendingEnabled=YES;
 pd.colorAttachments[0].sourceRGBBlendFactor=MTLBlendFactorOne;pd.colorAttachments[0].destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
 pd.colorAttachments[0].sourceAlphaBlendFactor=MTLBlendFactorOne;pd.colorAttachments[0].destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
 pipeline=[device newRenderPipelineStateWithDescriptor:pd error:&error];
 if(!pipeline) @throw [NSException exceptionWithName:@"PipelineError" reason:error.localizedDescription userInfo:nil];
 MTKTextureLoader* loader=[[MTKTextureLoader alloc]initWithDevice:device];
 atlasTexture=[loader newTextureWithContentsOfURL:[NSBundle.mainBundle URLForResource:@"atlas" withExtension:@"png"] options:@{MTKTextureLoaderOptionSRGB:@NO,MTKTextureLoaderOptionOrigin:MTKTextureLoaderOriginTopLeft} error:&error];
 if(!atlasTexture) @throw [NSException exceptionWithName:@"AtlasError" reason:error.localizedDescription userInfo:nil];
 MTLSamplerDescriptor* sd=[MTLSamplerDescriptor new];sd.minFilter=MTLSamplerMinMagFilterLinear;sd.magFilter=MTLSamplerMinMagFilterLinear;sd.sAddressMode=MTLSamplerAddressModeClampToEdge;sd.tAddressMode=MTLSamplerAddressModeClampToEdge;
 linearSampler=[device newSamplerStateWithDescriptor:sd];
 pacing.reset();
 inFlight=dispatch_semaphore_create(3);
 for(int i=0;i<3;++i)frameBuffers[i]=[device newBufferWithLength:4096*sizeof(Vertex) options:MTLResourceStorageModeShared];
 vertices.reserve(4096);previousDraws.reserve(128);renderQuads.reserve(128);
 std::ifstream f([NSBundle.mainBundle pathForResource:@"atlas" ofType:@"txt"].fileSystemRepresentation);
 std::string name;int w,h;float u,v,uw,vh;
 while(f>>name>>w>>h>>u>>v>>uw>>vh)game.atlas[name]={static_cast<int>(std::round(u*1024)),static_cast<int>(std::round(v*1024)),w,h};
 game.seed(static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()%32000));
 game.initialize(static_cast<int>([NSUserDefaults.standardUserDefaults integerForKey:@"highScore"]));
 game.draws.reserve(128);game.events.reserve(32);
 audio=[[GameAudio alloc]initWithDirectory:NSBundle.mainBundle.resourceURL error:&error];
 if(!audio)@throw[NSException exceptionWithName:@"AudioError" reason:error.localizedDescription userInfo:nil];
 [audio activate];
 [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(inactive:) name:UIApplicationWillResignActiveNotification object:nil];
 [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(active:) name:UIApplicationDidBecomeActiveNotification object:nil];
 self.fpsLabel=[UILabel new];self.fpsLabel.text=@"FPS —";self.fpsLabel.textColor=UIColor.whiteColor;
 self.fpsLabel.font=[UIFont monospacedDigitSystemFontOfSize:11 weight:UIFontWeightMedium];self.fpsLabel.textAlignment=NSTextAlignmentRight;
 self.fpsLabel.layer.shadowColor=UIColor.blackColor.CGColor;self.fpsLabel.layer.shadowOpacity=0.65;self.fpsLabel.layer.shadowRadius=1;self.fpsLabel.layer.shadowOffset=CGSizeMake(0,1);
 [self.view addSubview:self.fpsLabel];
 UITapGestureRecognizer* toggle=[[UITapGestureRecognizer alloc]initWithTarget:self action:@selector(toggleFPS:)];toggle.numberOfTouchesRequired=2;toggle.numberOfTapsRequired=2;toggle.cancelsTouchesInView=NO;[self.view addGestureRecognizer:toggle];

}
-(void)viewDidLayoutSubviews {
 [super viewDidLayoutSubviews];
 self.gameView.frame=self.view.bounds;
 layout=ViewLayout::make(self.view.bounds.size.width,self.view.bounds.size.height,self.view.safeAreaInsets.top,self.view.safeAreaInsets.bottom);
 game.bird.flapCeiling=layout.flightCeiling();
 CGFloat scale=self.view.window.screen.scale?:UIScreen.mainScreen.scale;
 pendingDrawableSize=CGSizeMake(self.gameView.bounds.size.width*scale,self.gameView.bounds.size.height*scale);
 self.gameView.layer.contentsScale=scale;
 if(!self.displayLink)((CAMetalLayer*)self.gameView.layer).drawableSize=pendingDrawableSize;
 self.fpsLabel.frame=CGRectMake(self.view.bounds.size.width-220,self.view.safeAreaInsets.top+4,208,22);
}
-(void)toggleFPS:(UITapGestureRecognizer*)recognizer {self.fpsLabel.hidden=!self.fpsLabel.hidden;}
-(void)viewDidAppear:(BOOL)animated {
 [super viewDidAppear:animated];
 if(!self.displayLink) {
  self.displayLink=[[CAMetalDisplayLink alloc]initWithMetalLayer:(CAMetalLayer*)self.gameView.layer];
  self.displayLink.delegate=self;self.displayLink.preferredFrameLatency=1;
  float maximum=MIN(120,self.view.window.screen.maximumFramesPerSecond);
  self.displayLink.preferredFrameRateRange=CAFrameRateRangeMake(maximum,maximum,maximum);
  [self.displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
 }
 self.displayLink.paused=NO;
}
-(void)metalDisplayLink:(CAMetalDisplayLink*)link needsUpdate:(CAMetalDisplayLinkUpdate*)update {
 @autoreleasepool {
 int ticks=clock.advance(update.targetPresentationTimestamp);
 for(int i=0;i<ticks;++i){previousDraws.assign(game.draws.begin(),game.draws.end());game.frame();[self handleEvents];}
 double now=CACurrentMediaTime();if(statsStart==0)statsStart=now;
 double interval=now-statsStart;
 if(interval>=1){auto stats=pacing.take();self.fpsLabel.text=[NSString stringWithFormat:@"%.0f FPS | P99 %.1f ms",stats.frames/interval,stats.p99Milliseconds];statsStart=now;}
 [self renderDrawable:update.drawable];
 }
}
-(BOOL)prefersStatusBarHidden{return YES;}
-(BOOL)prefersHomeIndicatorAutoHidden{return YES;}
-(UIInterfaceOrientationMask)supportedInterfaceOrientations{return UIInterfaceOrientationMaskPortrait;}
-(void)inactive:(NSNotification*)n {self.displayLink.paused=YES;game.touching=false;clock.reset();statsStart=0;pacing.reset();[audio pause];}
-(void)active:(NSNotification*)n {clock.reset();previousDraws.clear();statsStart=0;pacing.reset();[audio activate];self.displayLink.paused=NO;}
-(void)touchBegin:(CGPoint)p {game.touchBegin(layout.inputX(p.x),layout.inputY(p.y));}
-(void)touchUpdate:(CGPoint)p down:(BOOL)down {game.touchX=layout.inputX(p.x);game.touchY=layout.inputY(p.y);game.touching=down;}
-(void)handleEvents {
 for(int event:game.events){
  if(event>=9&&event<=13)[audio playEvent:event];
  if(event==0)[NSUserDefaults.standardUserDefaults setInteger:MAX(game.best,game.score) forKey:@"highScore"];
  if(event==1&&self.presentedViewController==nil){
   UIAlertController* a=[UIAlertController alertControllerWithTitle:@"Game Center" message:@"原版排行榜属于原开发者的 Game Center，个人签名无法访问。" preferredStyle:UIAlertControllerStyleAlert];
   [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];[self presentViewController:a animated:YES completion:nil];
  }
  if(event==2)[UIApplication.sharedApplication openURL:[NSURL URLWithString:@"https://itunes.apple.com/app/id642099621"] options:@{} completionHandler:nil];
 }
}
-(void)renderDrawable:(id<CAMetalDrawable>)drawable {
 @autoreleasepool {
 if(dispatch_semaphore_wait(inFlight,DISPATCH_TIME_NOW)!=0)return;
 MTLRenderPassDescriptor* pass=[MTLRenderPassDescriptor renderPassDescriptor];
 pass.colorAttachments[0].texture=drawable.texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
 pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,1);
 composeFrame(game,previousDraws,clock.blend(),layout,renderQuads);
 vertices.clear();
 for(const auto& r:renderQuads)appendQuad(vertices,r,layout.width,layout.height);
 id<MTLBuffer> buffer=frameBuffers[frameIndex];
 if(vertices.size()*sizeof(Vertex)>buffer.length){dispatch_semaphore_signal(inFlight);return;}
 memcpy(buffer.contents,vertices.data(),vertices.size()*sizeof(Vertex));
 id<MTLCommandBuffer> command=[queue commandBuffer];
 if(!command){dispatch_semaphore_signal(inFlight);return;}
 id<MTLRenderCommandEncoder> e=[command renderCommandEncoderWithDescriptor:pass];
 [e setRenderPipelineState:pipeline];[e setVertexBuffer:buffer offset:0 atIndex:0];
 [e setFragmentTexture:atlasTexture atIndex:0];[e setFragmentSamplerState:linearSampler atIndex:0];
 [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertices.size()];[e endEncoding];
 dispatch_semaphore_t semaphore=inFlight;
 [command addCompletedHandler:^(id<MTLCommandBuffer> completed){dispatch_semaphore_signal(semaphore);}];
 [drawable addPresentedHandler:^(id<MTLDrawable> presented){self->pacing.presented(presented.presentedTime);}];
 // CAMetalDisplayLink supplies the drawable and its presentation deadline.
 // Commit GPU work first, then use plain present() as required by its contract.
 [command commit];[drawable present];
 CAMetalLayer* layer=(CAMetalLayer*)self.gameView.layer;
 if(!CGSizeEqualToSize(layer.drawableSize,pendingDrawableSize))layer.drawableSize=pendingDrawableSize;
 frameIndex=(frameIndex+1)%3;
 }
}
@end
@interface SceneDelegate : UIResponder<UIWindowSceneDelegate>
@property(nonatomic,strong) UIWindow* window;
@end
@implementation SceneDelegate
-(void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options {
 if(![scene isKindOfClass:UIWindowScene.class])return;
 self.window=[[UIWindow alloc]initWithWindowScene:(UIWindowScene*)scene];
 self.window.rootViewController=[GameController new];[self.window makeKeyAndVisible];
}
@end
@interface AppDelegate : UIResponder<UIApplicationDelegate>
@end
@implementation AppDelegate
-(BOOL)application:(UIApplication*)app didFinishLaunchingWithOptions:(NSDictionary*)options {return YES;}
@end
int main(int argc,char** argv){@autoreleasepool{return UIApplicationMain(argc,argv,nil,NSStringFromClass(AppDelegate.class));}}
