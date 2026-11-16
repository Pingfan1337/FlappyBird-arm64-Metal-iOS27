#pragma once
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
struct AtlasPixels {NSUInteger width=0,height=0;NSMutableData* data=nil;};
static AtlasPixels LoadAtlasPixels(NSURL* url) {
    AtlasPixels out;
    CGImageSourceRef source=CGImageSourceCreateWithURL((__bridge CFURLRef)url,NULL);
    if(!source)return out;
    CGImageRef image=CGImageSourceCreateImageAtIndex(source,0,NULL);CFRelease(source);
    if(!image)return out;
    out.width=CGImageGetWidth(image);out.height=CGImageGetHeight(image);
    out.data=[NSMutableData dataWithLength:out.width*out.height*4];
    CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx=CGBitmapContextCreate(out.data.mutableBytes,out.width,out.height,8,out.width*4,space,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(space);
    if(ctx){CGContextDrawImage(ctx,CGRectMake(0,0,out.width,out.height),image);CGContextRelease(ctx);}else out.data=nil;
    CGImageRelease(image);return out;
}
