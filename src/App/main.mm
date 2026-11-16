#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#if __LP64__
#import <Metal/Metal.h>
#endif
#include "MetalGeometry.hpp"
#include "GLRenderer.hpp"
#include "AtlasPixels.hpp"
#include "../Core/FramePacingStats.hpp"
#include "AudioBackend.hpp"
#include "Shaders.hpp"
#include <fstream>
using namespace flappy12;
#if __LP64__
static float MaximumFPS(UIScreen* screen){
 if(@available(iOS 10.3,*))return MIN(120,screen.maximumFramesPerSecond);
 return 60;
}
#endif
@class GameController;
@interface GameView : UIView
@property(nonatomic,assign) GameController* owner;
@end
@interface GLGameView : GameView
@end
@implementation GLGameView
+(Class)layerClass{return CAEAGLLayer.class;}
@end
#if __LP64__
@interface MetalGameView : GameView
@end
@implementation MetalGameView
+(Class)layerClass{return CAMetalLayer.class;}
@end
#endif
@interface GameController : UIViewController {
@public
 OriginalGame game;PresentationClock clock;ViewLayout layout;FramePacingStats pacing;
 std::vector<Draw> previousDraws;std::vector<RenderQuad> renderQuads;std::vector<Vertex> vertices;
 GLRenderer glRenderer;BOOL useMetal;GameAudio* audio;CFTimeInterval statsStart;CGSize pendingDrawableSize;
#if __LP64__
 id<MTLCommandQueue> queue;id<MTLRenderPipelineState> pipeline;id<MTLTexture> atlasTexture;id<MTLSamplerState> sampler;
 id<MTLBuffer> frameBuffers[3];NSUInteger frameIndex;dispatch_semaphore_t inFlight;
#endif
}
@property(nonatomic,strong) GameView* gameView;
@property(nonatomic,strong) CADisplayLink* legacyLink;
@property(nonatomic,strong) UILabel* fpsLabel;
#if __LP64__
@property(nonatomic,strong) CAMetalDisplayLink* displayLink API_AVAILABLE(ios(17.0));
-(void)renderDrawable:(id<CAMetalDrawable>)drawable;
#endif
-(void)touchBegin:(CGPoint)p;
-(void)touchUpdate:(CGPoint)p down:(BOOL)down;
@end
@implementation GameView
-(void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {UITouch* t=[touches anyObject];CGPoint p=[t locationInView:self];[self.owner touchUpdate:p down:YES];[self.owner touchBegin:p];}
-(void)touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event {[self.owner touchUpdate:[[touches anyObject]locationInView:self] down:YES];}
-(void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {[self.owner touchUpdate:[[touches anyObject]locationInView:self] down:NO];}
-(void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event {[self.owner touchUpdate:CGPointZero down:NO];}
@end
@implementation GameController
-(void)loadView {
 self.view=[[UIView alloc]initWithFrame:UIScreen.mainScreen.bounds];self.view.backgroundColor=UIColor.blackColor;
 AtlasPixels atlas=LoadAtlasPixels([NSBundle.mainBundle URLForResource:@"atlas" withExtension:@"png"]);
 if(!atlas.data)@throw[NSException exceptionWithName:@"AtlasError" reason:@"Cannot decode atlas.png" userInfo:nil];
 NSError* error=nil;
#if __LP64__
 id<MTLDevice> device=nil;
 if(@available(iOS 8.0,*)){if(MTLCreateSystemDefaultDevice)device=MTLCreateSystemDefaultDevice();}
 useMetal=device!=nil;
 if(useMetal){
  self.gameView=[[MetalGameView alloc]initWithFrame:CGRectZero];
  CAMetalLayer* layer=(CAMetalLayer*)self.gameView.layer;layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.framebufferOnly=YES;layer.presentsWithTransaction=NO;layer.opaque=YES;
  if(@available(iOS 11.2,*))layer.maximumDrawableCount=3;
  queue=[device newCommandQueue];id<MTLLibrary> lib=[device newLibraryWithSource:@(gameShaderSource) options:nil error:&error];
  MTLRenderPipelineDescriptor* pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=[lib newFunctionWithName:@"sprite_vertex"];pd.fragmentFunction=[lib newFunctionWithName:@"sprite_fragment"];
  auto attachment=pd.colorAttachments[0];attachment.pixelFormat=MTLPixelFormatBGRA8Unorm;attachment.blendingEnabled=YES;attachment.sourceRGBBlendFactor=MTLBlendFactorOne;attachment.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;attachment.sourceAlphaBlendFactor=MTLBlendFactorOne;attachment.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
  pipeline=[device newRenderPipelineStateWithDescriptor:pd error:&error];if(!pipeline)@throw[NSException exceptionWithName:@"MetalError" reason:error.localizedDescription userInfo:nil];
  MTLTextureDescriptor* td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:atlas.width height:atlas.height mipmapped:NO];
  if(@available(iOS 9.0,*)){td.usage=MTLTextureUsageShaderRead;td.storageMode=MTLStorageModeShared;}
  atlasTexture=[device newTextureWithDescriptor:td];[atlasTexture replaceRegion:MTLRegionMake2D(0,0,atlas.width,atlas.height) mipmapLevel:0 withBytes:atlas.data.bytes bytesPerRow:atlas.width*4];
  MTLSamplerDescriptor* sd=[MTLSamplerDescriptor new];sd.minFilter=sd.magFilter=MTLSamplerMinMagFilterLinear;sd.sAddressMode=sd.tAddressMode=MTLSamplerAddressModeClampToEdge;sampler=[device newSamplerStateWithDescriptor:sd];
  inFlight=dispatch_semaphore_create(3);for(int i=0;i<3;++i)frameBuffers[i]=[device newBufferWithLength:4096*sizeof(Vertex) options:MTLResourceStorageModeShared];
 }
#endif
 if(!useMetal){self.gameView=[[GLGameView alloc]initWithFrame:CGRectZero];if(!glRenderer.initialize((CAEAGLLayer*)self.gameView.layer,atlas))@throw[NSException exceptionWithName:@"GLError" reason:@"OpenGL ES 2 renderer initialization failed" userInfo:nil];}
 self.gameView.owner=self;[self.view addSubview:self.gameView];
 vertices.reserve(4096);previousDraws.reserve(128);renderQuads.reserve(128);game.draws.reserve(128);game.events.reserve(32);pacing.reset();
 std::ifstream f([NSBundle.mainBundle pathForResource:@"atlas" ofType:@"txt"].fileSystemRepresentation);std::string n;int w,h;float u,v,uw,vh;
 while(f>>n>>w>>h>>u>>v>>uw>>vh)game.atlas[n]={int(std::round(u*1024)),int(std::round(v*1024)),w,h};
 game.seed((uint32_t)((long long)([NSDate timeIntervalSinceReferenceDate]*1000)%32000));game.initialize((int)[NSUserDefaults.standardUserDefaults integerForKey:@"highScore"]);
 audio=[[GameAudio alloc]initWithDirectory:NSBundle.mainBundle.resourceURL error:&error];if(!audio)@throw[NSException exceptionWithName:@"AudioError" reason:error.localizedDescription userInfo:nil];[audio activate];
 [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(inactive:) name:UIApplicationWillResignActiveNotification object:nil];[NSNotificationCenter.defaultCenter addObserver:self selector:@selector(active:) name:UIApplicationDidBecomeActiveNotification object:nil];
 self.fpsLabel=[UILabel new];self.fpsLabel.text=@"FPS —";self.fpsLabel.textColor=UIColor.whiteColor;
 if(@available(iOS 9.0,*))self.fpsLabel.font=[UIFont monospacedDigitSystemFontOfSize:11 weight:UIFontWeightMedium];else self.fpsLabel.font=[UIFont systemFontOfSize:11];
 self.fpsLabel.textAlignment=NSTextAlignmentRight;self.fpsLabel.backgroundColor=UIColor.clearColor;self.fpsLabel.layer.shadowColor=UIColor.blackColor.CGColor;self.fpsLabel.layer.shadowOpacity=.65;self.fpsLabel.layer.shadowRadius=1;self.fpsLabel.layer.shadowOffset=CGSizeMake(0,1);[self.view addSubview:self.fpsLabel];
 UITapGestureRecognizer* toggle=[[UITapGestureRecognizer alloc]initWithTarget:self action:@selector(toggleFPS:)];toggle.numberOfTouchesRequired=2;toggle.numberOfTapsRequired=2;toggle.cancelsTouchesInView=NO;[self.view addGestureRecognizer:toggle];
}
-(void)viewDidLayoutSubviews {
 [super viewDidLayoutSubviews];self.gameView.frame=self.view.bounds;UIEdgeInsets safe=UIEdgeInsetsZero;
#if __LP64__
 if(@available(iOS 11.0,*))safe=self.view.safeAreaInsets;
#endif
 layout=ViewLayout::make(self.view.bounds.size.width,self.view.bounds.size.height,safe.top,safe.bottom);game.bird.flapCeiling=layout.flightCeiling();
 CGFloat scale=(self.view.window?self.view.window.screen:UIScreen.mainScreen).scale;pendingDrawableSize=CGSizeMake(self.view.bounds.size.width*scale,self.view.bounds.size.height*scale);self.gameView.layer.contentsScale=scale;
#if __LP64__
 if(useMetal){BOOL modernLink=NO;if(@available(iOS 17.0,*))modernLink=self.displayLink!=nil;if(!modernLink)((CAMetalLayer*)self.gameView.layer).drawableSize=pendingDrawableSize;}else
#endif
 glRenderer.resize((CAEAGLLayer*)self.gameView.layer);
 self.fpsLabel.frame=CGRectMake(self.view.bounds.size.width-220,safe.top+4,208,22);
}
-(void)viewDidAppear:(BOOL)animated {
 [super viewDidAppear:animated];
#if __LP64__
 if(useMetal){if(@available(iOS 17.0,*)){
  if(!self.displayLink){self.displayLink=[[CAMetalDisplayLink alloc]initWithMetalLayer:(CAMetalLayer*)self.gameView.layer];self.displayLink.delegate=(id<CAMetalDisplayLinkDelegate>)self;self.displayLink.preferredFrameLatency=1;float hz=MaximumFPS(self.view.window.screen);self.displayLink.preferredFrameRateRange=CAFrameRateRangeMake(hz,hz,hz);[self.displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];}
  if(@available(iOS 17.0,*))self.displayLink.paused=NO;return;
 }}
#endif
 if(!self.legacyLink){self.legacyLink=[CADisplayLink displayLinkWithTarget:self selector:@selector(legacyTick:)];
#if __LP64__
  float hz=MaximumFPS(self.view.window.screen);
  if(@available(iOS 15.0,*))self.legacyLink.preferredFrameRateRange=CAFrameRateRangeMake(hz,hz,hz);
  else if(@available(iOS 10.0,*))self.legacyLink.preferredFramesPerSecond=(NSInteger)hz;else self.legacyLink.frameInterval=1;
#else
  self.legacyLink.frameInterval=1;
#endif
  [self.legacyLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
 }
 self.legacyLink.paused=NO;
}
-(void)advance:(CFTimeInterval)target {
 int ticks=clock.advance(target);for(int i=0;i<ticks;++i){previousDraws.assign(game.draws.begin(),game.draws.end());game.frame();[self handleEvents];}
 double now=CACurrentMediaTime();if(statsStart==0)statsStart=now;double interval=now-statsStart;
 if(interval>=1){auto stats=pacing.take();self.fpsLabel.text=[NSString stringWithFormat:@"%@ %.0f FPS | %.1f ms",useMetal?@"Metal":@"GLES",stats.frames/interval,stats.p99Milliseconds];statsStart=now;}
}
-(void)legacyTick:(CADisplayLink*)link {
 @autoreleasepool {
 CFTimeInterval target=link.timestamp+link.duration;
#if __LP64__
 if(@available(iOS 10.0,*))target=link.targetTimestamp;
#endif
 [self advance:target];
#if __LP64__
 if(useMetal){id<CAMetalDrawable> drawable=[(CAMetalLayer*)self.gameView.layer nextDrawable];if(drawable)[self renderDrawable:drawable];return;}
#endif
 composeFrame(game,previousDraws,clock.blend(),layout,renderQuads);vertices.clear();for(auto& q:renderQuads)appendQuad(vertices,q,layout.width,layout.height);glRenderer.draw(vertices);pacing.presented(CACurrentMediaTime());
 }
}
#if __LP64__
-(void)metalDisplayLink:(CAMetalDisplayLink*)link needsUpdate:(CAMetalDisplayLinkUpdate*)update API_AVAILABLE(ios(17.0)) { @autoreleasepool{[self advance:update.targetPresentationTimestamp];[self renderDrawable:update.drawable];} }
-(void)renderDrawable:(id<CAMetalDrawable>)drawable {
 @autoreleasepool {
 if(dispatch_semaphore_wait(inFlight,DISPATCH_TIME_NOW)!=0)return;
 MTLRenderPassDescriptor* pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=drawable.texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,1);
 composeFrame(game,previousDraws,clock.blend(),layout,renderQuads);vertices.clear();for(auto& q:renderQuads)appendQuad(vertices,q,layout.width,layout.height);
 id<MTLBuffer> buffer=frameBuffers[frameIndex];if(vertices.size()*sizeof(Vertex)>buffer.length){dispatch_semaphore_signal(inFlight);return;}memcpy(buffer.contents,vertices.data(),vertices.size()*sizeof(Vertex));
 id<MTLCommandBuffer> command=[queue commandBuffer];if(!command){dispatch_semaphore_signal(inFlight);return;}
 id<MTLRenderCommandEncoder> e=[command renderCommandEncoderWithDescriptor:pass];[e setRenderPipelineState:pipeline];[e setVertexBuffer:buffer offset:0 atIndex:0];[e setFragmentTexture:atlasTexture atIndex:0];[e setFragmentSamplerState:sampler atIndex:0];[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertices.size()];[e endEncoding];
 dispatch_semaphore_t semaphore=inFlight;[command addCompletedHandler:^(id<MTLCommandBuffer> done){dispatch_semaphore_signal(semaphore);}];
 if(@available(iOS 10.3,*))[drawable addPresentedHandler:^(id<MTLDrawable> done){self->pacing.presented(done.presentedTime);}];else [command addCompletedHandler:^(id<MTLCommandBuffer> done){self->pacing.presented(CACurrentMediaTime());}];
 BOOL modernLink=NO;if(@available(iOS 17.0,*))modernLink=self.displayLink!=nil;
 if(modernLink){[command commit];[drawable present];}else{[command presentDrawable:drawable];[command commit];}
 CAMetalLayer* layer=(CAMetalLayer*)self.gameView.layer;if(!CGSizeEqualToSize(layer.drawableSize,pendingDrawableSize))layer.drawableSize=pendingDrawableSize;frameIndex=(frameIndex+1)%3;
 }
}
#endif
-(void)toggleFPS:(UITapGestureRecognizer*)recognizer {self.fpsLabel.hidden=!self.fpsLabel.hidden;}
-(BOOL)prefersStatusBarHidden{return YES;}
-(BOOL)prefersHomeIndicatorAutoHidden{return YES;}
-(UIInterfaceOrientationMask)supportedInterfaceOrientations{return UIInterfaceOrientationMaskPortrait;}
-(BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)orientation{return orientation==UIInterfaceOrientationPortrait;}
-(void)inactive:(NSNotification*)n {
#if __LP64__
 if(@available(iOS 17.0,*))self.displayLink.paused=YES;
#endif
 self.legacyLink.paused=YES;game.touching=false;clock.reset();statsStart=0;pacing.reset();[audio pause];
}
-(void)active:(NSNotification*)n {clock.reset();previousDraws.clear();statsStart=0;pacing.reset();[audio activate];
#if __LP64__
 if(@available(iOS 17.0,*))self.displayLink.paused=NO;
#endif
 self.legacyLink.paused=NO;
}
-(void)touchBegin:(CGPoint)p {game.touchBegin(layout.inputX(p.x),layout.inputY(p.y));}
-(void)touchUpdate:(CGPoint)p down:(BOOL)down {game.touchX=layout.inputX(p.x);game.touchY=layout.inputY(p.y);game.touching=down;}
-(void)handleEvents {
 for(int event:game.events){if(event>=9&&event<=13)[audio playEvent:event];if(event==0)[NSUserDefaults.standardUserDefaults setInteger:MAX(game.best,game.score) forKey:@"highScore"];
  if(event==1){if(@available(iOS 8.0,*)){if(self.presentedViewController==nil){UIAlertController* a=[UIAlertController alertControllerWithTitle:@"Game Center" message:@"原版排行榜属于原开发者的 Game Center，个人签名无法访问。" preferredStyle:UIAlertControllerStyleAlert];[a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];[self presentViewController:a animated:YES completion:nil];}}
  else{UIAlertView* a=[[UIAlertView alloc]initWithTitle:@"Game Center" message:@"Original leaderboard unavailable." delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil];[a show];}}
  if(event==2){NSURL* url=[NSURL URLWithString:@"https://itunes.apple.com/app/id642099621"];
#if __LP64__
   if(@available(iOS 10.0,*))[UIApplication.sharedApplication openURL:url options:@{} completionHandler:nil];else
#endif
   [UIApplication.sharedApplication openURL:url];}
 }
}
@end
#if __LP64__
API_AVAILABLE(ios(13.0))
@interface SceneDelegate : UIResponder<UIWindowSceneDelegate>
@property(nonatomic,strong) UIWindow* window;
@end
@implementation SceneDelegate
-(void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options {if(@available(iOS 13.0,*)){if(![scene isKindOfClass:UIWindowScene.class])return;self.window=[[UIWindow alloc]initWithWindowScene:(UIWindowScene*)scene];self.window.rootViewController=[GameController new];[self.window makeKeyAndVisible];}}
@end
#endif
@interface AppDelegate : UIResponder<UIApplicationDelegate>
@property(nonatomic,strong) UIWindow* window;
@end
@implementation AppDelegate
-(BOOL)application:(UIApplication*)app didFinishLaunchingWithOptions:(NSDictionary*)options {
 [UIApplication.sharedApplication setStatusBarHidden:YES];
#if __LP64__
 if(@available(iOS 13.0,*))return YES;
#endif
 self.window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];self.window.rootViewController=[GameController new];[self.window makeKeyAndVisible];return YES;
}
@end
int main(int argc,char** argv){@autoreleasepool{return UIApplicationMain(argc,argv,nil,NSStringFromClass(AppDelegate.class));}}
