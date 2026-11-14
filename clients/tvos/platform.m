/* SPDX-License-Identifier: MIT
 * UIKit scene integration follows SDL2's supported app-delegate subclass hook.
 * Based on the MIT-licensed OpenFP tvOS platform integration, Tommy Frössman. */
#import <UIKit/UIKit.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include "tvos_platform.h"
@interface SDLUIKitDelegate : NSObject <UIApplicationDelegate>
+ (NSString *)getAppDelegateClassName;
- (void)postFinishLaunch;
@end
static UIWindowScene *game_scene;
static __weak UIWindow *game_window;
static int editing;
@interface LCSceneDelegate : NSObject <UIWindowSceneDelegate> @end
@interface LCAppDelegate : SDLUIKitDelegate @end
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-protocol-method-implementation"
@implementation SDLUIKitDelegate (LCDelegateClass)
+ (NSString *)getAppDelegateClassName { return @"LCAppDelegate"; }
@end
#pragma clang diagnostic pop
@implementation LCAppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    BOOL result = [super application:application didFinishLaunchingWithOptions:options];
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(postFinishLaunch) object:nil];
    return result;
}
@end
@implementation LCSceneDelegate
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options {
    (void)session; (void)options;
    if (![scene isKindOfClass:[UIWindowScene class]]) return;
    game_scene = (UIWindowScene *)scene;
    [(LCAppDelegate *)UIApplication.sharedApplication.delegate performSelector:@selector(postFinishLaunch) withObject:nil afterDelay:0];
}
- (void)sceneDidDisconnect:(UIScene *)scene { if (scene == game_scene) game_scene = nil; }
@end
void lc_tvos_attach_window(SDL_Window *window) {
    SDL_SysWMinfo info; SDL_VERSION(&info.version);
    if (game_scene && SDL_GetWindowWMInfo(window, &info)) {
        game_window = info.info.uikit.window;
        game_window.windowScene = game_scene;
        game_window.frame = game_scene.coordinateSpace.bounds;
        [game_window makeKeyAndVisible];
    }
}
void lc_tvos_data_path(char *out, size_t capacity, const char *name) {
    @autoreleasepool {
        NSURL *base = [[NSFileManager defaultManager] URLsForDirectory:NSCachesDirectory inDomains:NSUserDomainMask].firstObject;
        NSURL *folder = [base URLByAppendingPathComponent:@"LineClash" isDirectory:YES];
        [[NSFileManager defaultManager] createDirectoryAtURL:folder withIntermediateDirectories:YES attributes:nil error:nil];
        snprintf(out, capacity, "%s/%s", folder.path.UTF8String, name);
    }
}
int lc_tvos_editing(void) { return editing; }
/* Keep the UIKit editor alive until its completion; client buffers have static lifetime. */
@interface LCTextEditor : NSObject <UITextFieldDelegate>
@property(nonatomic) UIAlertController *dialog;
@property(nonatomic) char *buffer;
@property(nonatomic) size_t capacity;
@property(nonatomic) int *length;
- (void)finish:(BOOL)save;
@end
static LCTextEditor *active_editor;
@implementation LCTextEditor
- (BOOL)textFieldShouldReturn:(UITextField *)field { (void)field;[self finish:YES];return YES; }
- (void)finish:(BOOL)save {
    if(!editing)return;
    UITextField *field=self.dialog.textFields.firstObject;
    if(save){
        NSString *value=field.text ?: @"";
        while([value lengthOfBytesUsingEncoding:NSUTF8StringEncoding]>=self.capacity && value.length){
            NSRange last=[value rangeOfComposedCharacterSequenceAtIndex:value.length-1];value=[value substringToIndex:last.location];
        }
        snprintf(self.buffer,self.capacity,"%s",value.UTF8String ?: "");*self.length=(int)strlen(self.buffer);
    }
    field.delegate=nil;field.text=@"";
    UIAlertController *dialog=self.dialog;self.dialog=nil;
    /* Dismiss from the presenting controller so the full-screen keyboard and
     * its alert both close before SDL receives input again. */
    UIViewController *presenter=dialog.presentingViewController ?: game_window.rootViewController;
    [presenter dismissViewControllerAnimated:YES completion:^{
        editing=0;SDL_StopTextInput();SDL_SetHint(SDL_HINT_APPLE_TV_CONTROLLER_UI_EVENTS,"0");active_editor=nil;
    }];
}
@end
void lc_tvos_edit(char *text,size_t capacity,int *length,int password,const char *title) {
    if(editing || !capacity || !game_window.rootViewController)return;
    editing=1;SDL_StartTextInput();SDL_SetHint(SDL_HINT_APPLE_TV_CONTROLLER_UI_EVENTS,"1");
    LCTextEditor *editor=[LCTextEditor new];active_editor=editor;editor.buffer=text;editor.capacity=capacity;editor.length=length;
    UIAlertController *dialog=[UIAlertController alertControllerWithTitle:@(title) message:nil preferredStyle:UIAlertControllerStyleAlert];editor.dialog=dialog;
    [dialog addTextFieldWithConfigurationHandler:^(UITextField *field){
        field.text=@(text);field.secureTextEntry=password;field.delegate=editor;
        field.autocorrectionType=UITextAutocorrectionTypeNo;field.autocapitalizationType=UITextAutocapitalizationTypeNone;
        field.returnKeyType=UIReturnKeyDone;
    }];
    [dialog addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:^(UIAlertAction *action){(void)action;[editor finish:NO];}]];
    UIAlertAction *done=[UIAlertAction actionWithTitle:@"Done" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action){(void)action;[editor finish:YES];}];
    [dialog addAction:done];dialog.preferredAction=done;
    [game_window.rootViewController presentViewController:dialog animated:YES completion:^{[dialog.textFields.firstObject becomeFirstResponder];}];
}
