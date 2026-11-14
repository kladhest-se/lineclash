/* SPDX-License-Identifier: MIT
 * Foundation HTTPS with a bounded response, default certificate validation,
 * no redirects, cookies, credentials cache or persistent URL cache.
 * Derived from OpenFP's MIT-licensed tvOS HTTP bridge, Tommy Frössman. */
#import <Foundation/Foundation.h>
#include "tvos_platform.h"
#include <string.h>
@interface LCHTTPReader : NSObject <NSURLSessionDataDelegate>
@property(nonatomic) NSMutableData *data;
@property(nonatomic) NSUInteger capacity;
@property(nonatomic) NSInteger status;
@property(nonatomic) dispatch_semaphore_t finished;
@end
@implementation LCHTTPReader
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))finish {
    (void)session;(void)task;(void)response;(void)request;finish(nil);
}
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)task didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition))finish {
    (void)session;(void)task;
    self.status=[(NSHTTPURLResponse *)response statusCode];
    if (response.expectedContentLength>=0 && (unsigned long long)response.expectedContentLength>=self.capacity) { self.status=-1;finish(NSURLSessionResponseCancel); }
    else finish(NSURLSessionResponseAllow);
}
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)task didReceiveData:(NSData *)data {
    (void)session;
    if (data.length>=self.capacity-self.data.length || memchr(data.bytes,0,data.length)) { self.status=-1;[task cancel]; }
    else [self.data appendData:data];
}
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error {
    (void)session;(void)task;if(error)self.status=-1;dispatch_semaphore_signal(self.finished);
}
@end
/* Runs on an SDL worker, never blocks the UIKit thread. */
int lc_tvos_http(const char *url,const char *body,char *out,size_t capacity,int timeout_ms) {
    @autoreleasepool {
        if(!url || !out || !capacity || timeout_ms<=0)return -1;
        out[0]=0;NSURL *address=[NSURL URLWithString:@(url)];
        if(![address.scheme isEqualToString:@"https"] || address.user || address.password)return -1;
        LCHTTPReader *reader=[LCHTTPReader new];reader.capacity=capacity;reader.status=-1;
        reader.data=[NSMutableData data];reader.finished=dispatch_semaphore_create(0);
        NSURLSessionConfiguration *config=[NSURLSessionConfiguration ephemeralSessionConfiguration];
        config.timeoutIntervalForRequest=config.timeoutIntervalForResource=timeout_ms/1000.0;
        config.HTTPCookieStorage=nil;config.URLCredentialStorage=nil;config.URLCache=nil;
        NSOperationQueue *queue=[NSOperationQueue new];queue.maxConcurrentOperationCount=1;
        NSURLSession *session=[NSURLSession sessionWithConfiguration:config delegate:reader delegateQueue:queue];
        NSMutableURLRequest *request=[NSMutableURLRequest requestWithURL:address];
        [request setValue:@"LineClash tvOS/0.1" forHTTPHeaderField:@"User-Agent"];
        if(body){request.HTTPMethod=@"POST";request.HTTPBody=[NSData dataWithBytes:body length:strlen(body)];[request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];}
        NSURLSessionDataTask *task=[session dataTaskWithRequest:request];[task resume];
        if(dispatch_semaphore_wait(reader.finished,dispatch_time(DISPATCH_TIME_NOW,(int64_t)(timeout_ms+1000)*1000000))!=0){
            [session invalidateAndCancel];return -1;
        }
        int status=(int)reader.status;
        if(status>0){memcpy(out,reader.data.bytes,reader.data.length);out[reader.data.length]=0;}
        [session finishTasksAndInvalidate];return status;
    }
}
