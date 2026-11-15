#pragma once
#import <AVFoundation/AVFoundation.h>
#import <TargetConditionals.h>
#include <cstring>
#include <cstdint>

// Decode once at load time. All supplied assets are 44.1kHz PCM, and playback
// always uses the same stereo float format, without per-event seeking/decoding.
static AVAudioPCMBuffer* LoadGameSound(NSURL* url,NSError** error) {
    AVAudioFile* file=[[AVAudioFile alloc]initForReading:url commonFormat:AVAudioPCMFormatFloat32 interleaved:NO error:error];
    if(!file)return nil;
    if(file.processingFormat.sampleRate!=44100||file.length>UINT32_MAX){
        if(error)*error=[NSError errorWithDomain:@"FlappyAudio" code:1 userInfo:@{NSLocalizedDescriptionKey:@"Unexpected source audio format"}];
        return nil;
    }
    AVAudioPCMBuffer* source=[[AVAudioPCMBuffer alloc]initWithPCMFormat:file.processingFormat frameCapacity:static_cast<AVAudioFrameCount>(file.length)];
    if(![file readIntoBuffer:source error:error])return nil;
    AVAudioFormat* stereo=[[AVAudioFormat alloc]initStandardFormatWithSampleRate:44100 channels:2];
    AVAudioPCMBuffer* output=[[AVAudioPCMBuffer alloc]initWithPCMFormat:stereo frameCapacity:source.frameLength];
    output.frameLength=source.frameLength;
    for(int channel=0;channel<2;++channel)memcpy(output.floatChannelData[channel],source.floatChannelData[MIN(channel,static_cast<int>(source.format.channelCount)-1)],source.frameLength*sizeof(float));
    return output;
}
@interface GameAudio : NSObject {
    AVAudioEngine* engine;
    AVAudioPlayerNode* nodes[3];
    AVAudioPCMBuffer* buffers[5];
    dispatch_queue_t audioQueue;
}
-(instancetype)initWithDirectory:(NSURL*)directory error:(NSError**)error;
-(void)activate;
-(void)pause;
-(void)playEvent:(int)event;
@end
@implementation GameAudio
-(instancetype)initWithDirectory:(NSURL*)directory error:(NSError**)error {
    if(!(self=[super init]))return nil;
    audioQueue=dispatch_queue_create("flappy.audio",dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,QOS_CLASS_USER_INTERACTIVE,0));
    NSArray* names=@[@"sfx_point.aif",@"sfx_swooshing.caf",@"sfx_die.caf",@"sfx_hit.caf",@"sfx_wing.caf"];
    for(int i=0;i<5;++i){buffers[i]=LoadGameSound([directory URLByAppendingPathComponent:names[i]],error);if(!buffers[i])return nil;}
    engine=[AVAudioEngine new];
    for(int i=0;i<3;++i){nodes[i]=[AVAudioPlayerNode new];[engine attachNode:nodes[i]];[engine connect:nodes[i] to:engine.mainMixerNode format:buffers[0].format];}
    [engine prepare];return self;
}
-(void)activate {
    dispatch_async(audioQueue,^{
        NSError* error=nil;
#if TARGET_OS_IOS
        [AVAudioSession.sharedInstance setCategory:AVAudioSessionCategoryAmbient error:&error];
        [AVAudioSession.sharedInstance setPreferredIOBufferDuration:128.0/44100 error:nil];
        [AVAudioSession.sharedInstance setActive:YES error:&error];
#endif
        if(!self->engine.running&&![self->engine startAndReturnError:&error]){NSLog(@"Audio startup: %@",error);return;}
        for(int i=0;i<3;++i)if(!self->nodes[i].playing)[self->nodes[i] play];
    });
}
-(void)pause {dispatch_async(audioQueue,^{[self->engine pause];});}
-(void)playEvent:(int)event {
    if(event<9||event>13)return;
    dispatch_async(audioQueue,^{
        if(!self->engine.running)return;
        int channel=(event==9||event==12)?0:event==10?1:2;
        AVAudioPlayerNode* node=self->nodes[channel];node.volume=event==9?.2f:1.f;
        [node scheduleBuffer:self->buffers[event-9] atTime:nil options:AVAudioPlayerNodeBufferInterrupts completionHandler:nil];
        if(!node.playing)[node play];
    });
}
@end
