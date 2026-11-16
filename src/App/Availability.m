#import <UIKit/UIKit.h>
#include <stdint.h>
// clang's availability helper in current compiler-rt imports newer libSystem
// entry points. Resolve its call locally using APIs present on iOS 5 instead.
int __isPlatformVersionAtLeast(uint32_t platform,uint32_t major,uint32_t minor,uint32_t patch) {
    (void)platform;
    static uint32_t actual[3];static dispatch_once_t once;
    dispatch_once(&once,^{NSArray* parts=[[[UIDevice currentDevice] systemVersion] componentsSeparatedByString:@"."];for(NSUInteger i=0;i<3&&i<[parts count];++i)actual[i]=(uint32_t)[[parts objectAtIndex:i] integerValue];});
    if(actual[0]!=major)return actual[0]>major;
    if(actual[1]!=minor)return actual[1]>minor;
    return actual[2]>=patch;
}
int __isOSVersionAtLeast(int major,int minor,int patch){return __isPlatformVersionAtLeast(2,major,minor,patch);}
