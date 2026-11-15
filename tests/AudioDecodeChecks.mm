#import "../src/App/AudioBackend.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){@autoreleasepool{
 NSArray* names=@[@"sfx_point.aif",@"sfx_swooshing.caf",@"sfx_die.caf",@"sfx_hit.caf",@"sfx_wing.caf"];
 for(NSString* name in names){NSError* error=nil;NSURL* url=[NSURL fileURLWithPath:[@"resources" stringByAppendingPathComponent:name]];AVAudioPCMBuffer* b=LoadGameSound(url,&error);assert(b&&b.frameLength>0&&b.format.channelCount==2&&b.format.sampleRate==44100);double energy=0;
 for(unsigned c=0;c<2;++c)for(unsigned i=0;i<b.frameLength;++i){float sample=b.floatChannelData[c][i];assert(std::isfinite(sample));energy+=sample*sample;}
 assert(energy>0);if([name isEqualToString:@"sfx_wing.caf"])for(unsigned i=0;i<b.frameLength;++i)assert(b.floatChannelData[0][i]==b.floatChannelData[1][i]);
 std::cout<<name.UTF8String<<": "<<b.frameLength<<" stereo float PCM frames validated\n";
 }
}}
