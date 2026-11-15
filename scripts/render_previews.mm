#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>
#import <ImageIO/ImageIO.h>
#include "../src/App/MetalGeometry.hpp"
#include "../src/App/Shaders.hpp"
#include <fstream>
#include <iostream>
using namespace flappy12;
int main(){@autoreleasepool{
 id<MTLDevice> device=MTLCreateSystemDefaultDevice();if(!device){std::cerr<<"A local Metal device is required\n";return 2;}
 NSError* error=nil;id<MTLLibrary> lib=[device newLibraryWithSource:@(gameShaderSource) options:nil error:&error];
 MTLRenderPipelineDescriptor* pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=[lib newFunctionWithName:@"sprite_vertex"];pd.fragmentFunction=[lib newFunctionWithName:@"sprite_fragment"];
 auto c=pd.colorAttachments[0];c.pixelFormat=MTLPixelFormatBGRA8Unorm;c.blendingEnabled=YES;c.sourceRGBBlendFactor=MTLBlendFactorOne;c.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;c.sourceAlphaBlendFactor=MTLBlendFactorOne;c.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
 id<MTLRenderPipelineState> pipeline=[device newRenderPipelineStateWithDescriptor:pd error:&error];if(!pipeline){NSLog(@"%@",error);return 1;}
 MTKTextureLoader* loader=[[MTKTextureLoader alloc]initWithDevice:device];id<MTLTexture> atlas=[loader newTextureWithContentsOfURL:[NSURL fileURLWithPath:@"resources/atlas.png"] options:@{MTKTextureLoaderOptionSRGB:@NO,MTKTextureLoaderOptionOrigin:MTKTextureLoaderOriginTopLeft} error:&error];if(!atlas)return 3;
 MTLTextureDescriptor* td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:1320 height:2868 mipmapped:NO];td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageRenderTarget;id<MTLTexture> target=[device newTextureWithDescriptor:td];
 MTLSamplerDescriptor* sd=[MTLSamplerDescriptor new];sd.minFilter=sd.magFilter=MTLSamplerMinMagFilterLinear;sd.sAddressMode=sd.tAddressMode=MTLSamplerAddressModeClampToEdge;id<MTLSamplerState> sampler=[device newSamplerStateWithDescriptor:sd];
 id<MTLCommandQueue> queue=[device newCommandQueue];id<MTLBuffer> buffer=[device newBufferWithLength:4096*sizeof(Vertex) options:MTLResourceStorageModeShared];
 OriginalGame game;std::ifstream f("resources/atlas.txt");std::string n;int w,h;float u,v,uw,vh;while(f>>n>>w>>h>>u>>v>>uw>>vh)game.atlas[n]={int(u*1024+.5),int(v*1024+.5),w,h};
 ViewLayout layout=ViewLayout::make(440,956,62,34);game.bird.flapCeiling=layout.flightCeiling();game.seed(1234);std::srand(1);
 auto save=[&](NSString* name){
  std::vector<RenderQuad> quads;composeFrame(game,game.draws,1,layout,quads);std::vector<Vertex> vertices;for(auto& q:quads)appendQuad(vertices,q,layout.width,layout.height);
  memcpy(buffer.contents,vertices.data(),vertices.size()*sizeof(Vertex));id<MTLCommandBuffer> cb=[queue commandBuffer];MTLRenderPassDescriptor* pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
  id<MTLRenderCommandEncoder> e=[cb renderCommandEncoderWithDescriptor:pass];[e setRenderPipelineState:pipeline];[e setVertexBuffer:buffer offset:0 atIndex:0];[e setFragmentTexture:atlas atIndex:0];[e setFragmentSamplerState:sampler atIndex:0];[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertices.size()];[e endEncoding];[cb commit];[cb waitUntilCompleted];if(cb.error)@throw[NSException exceptionWithName:@"GPU" reason:cb.error.localizedDescription userInfo:nil];
  std::vector<uint8_t> pixels(1320*2868*4);[target getBytes:pixels.data() bytesPerRow:1320*4 fromRegion:MTLRegionMake2D(0,0,1320,2868) mipmapLevel:0];CGColorSpaceRef cs=CGColorSpaceCreateDeviceRGB();CGContextRef ctx=CGBitmapContextCreate(pixels.data(),1320,2868,8,1320*4,cs,kCGImageAlphaPremultipliedFirst|kCGBitmapByteOrder32Little);CGImageRef image=CGBitmapContextCreateImage(ctx);
  NSURL* url=[NSURL fileURLWithPath:[@"docs/images" stringByAppendingPathComponent:[name stringByAppendingPathExtension:@"png"]]];CGImageDestinationRef dst=CGImageDestinationCreateWithURL((__bridge CFURLRef)url,CFSTR("public.png"),1,nil);CGImageDestinationAddImage(dst,image,nil);if(!CGImageDestinationFinalize(dst))abort();CFRelease(dst);CGImageRelease(image);CGContextRelease(ctx);CGColorSpaceRelease(cs);
  std::cout<<name.UTF8String<<" / score="<<game.score<<"\n";
 };
 game.initialize(0);for(int i=0;i<90;++i)game.frame();save(@"menu");
 game.handle(5);for(int i=0;i<31;++i)game.frame();save(@"ready");game.touchBegin(150,250);
 bool captured=false;
 for(int i=0;i<1800&&game.speed>0;++i){
  int goal=216;for(int p=0;p<3;++p)if(game.pipeX[p]+52>=game.bird.x){goal=game.pipeY[p]-58;break;}
  if(game.bird.y>=goal+15&&game.bird.velocity>=0)game.flap();game.frame();
  if(game.score>=2&&game.speed>0){save(@"gameplay");captured=true;break;}
 }
 if(!captured){std::cerr<<"Autoplay capture: score="<<game.score<<" birdY="<<game.bird.y<<" pipeY="<<game.pipeY[0]<<" pipeX="<<game.pipeX[0]<<"\n";return 4;}
 for(int i=0;i<400;++i)game.frame();save(@"results");
}}
