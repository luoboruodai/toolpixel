#import <Cocoa/Cocoa.h>

#include "ToonMath.h"

@interface ToonPixelColorPickerDelegate : NSObject <NSWindowDelegate>
@property(nonatomic) BOOL accepted;
@property(nonatomic) BOOL finished;
- (void)accept:(id)sender;
- (void)cancel:(id)sender;
@end

@implementation ToonPixelColorPickerDelegate

- (void)accept:(id)sender {
    if (self.finished) return;
    self.accepted = YES;
    self.finished = YES;
    [NSApp stopModal];
}

- (void)cancel:(id)sender {
    if (self.finished) return;
    self.accepted = NO;
    self.finished = YES;
    [NSApp stopModal];
}

- (void)windowWillClose:(NSNotification*)notification {
    if (self.finished) return;
    self.accepted = NO;
    self.finished = YES;
    [NSApp stopModal];
}

@end

extern "C" int ToonPixelPickColor(const char* title, float* red, float* green, float* blue) {
    if (!title || !red || !green || !blue || ![NSThread isMainThread]) return -1;
    @autoreleasepool {
        NSApplication* application = [NSApplication sharedApplication];
        NSColorPanel* panel = [NSColorPanel sharedColorPanel];
        ToonPixelColorPickerDelegate* delegate = [[ToonPixelColorPickerDelegate alloc] init];
        id previousDelegate = panel.delegate;
        NSView* previousAccessory = panel.accessoryView;
        NSString* previousTitle = panel.title;
        NSColor* previousColor = panel.color;
        const BOOL previousAlpha = panel.showsAlpha;
        const BOOL previousWorksWhenModal = panel.worksWhenModal;
        const BOOL previousHidesOnDeactivate = panel.hidesOnDeactivate;

        NSButton* cancelButton = [NSButton buttonWithTitle:@"Cancel"
                                                    target:delegate
                                                    action:@selector(cancel:)];
        cancelButton.keyEquivalent = @"\x1b";
        cancelButton.frame = NSMakeRect(62.0, 4.0, 64.0, 24.0);
        cancelButton.autoresizingMask = NSViewMinXMargin;
        NSButton* acceptButton = [NSButton buttonWithTitle:@"OK"
                                                    target:delegate
                                                    action:@selector(accept:)];
        acceptButton.keyEquivalent = @"\r";
        acceptButton.frame = NSMakeRect(132.0, 4.0, 64.0, 24.0);
        acceptButton.autoresizingMask = NSViewMinXMargin;
        NSView* actions = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 200.0, 32.0)];
        actions.autoresizingMask = NSViewWidthSizable;
        [actions addSubview:cancelButton];
        [actions addSubview:acceptButton];

        int result = 1;
        @try {
            panel.delegate = delegate;
            panel.accessoryView = actions;
            panel.title = [NSString stringWithUTF8String:title];
            panel.color = [NSColor colorWithSRGBRed:toonpixel::encode_unmanaged(*red)
                                              green:toonpixel::encode_unmanaged(*green)
                                               blue:toonpixel::encode_unmanaged(*blue)
                                              alpha:1.0];
            panel.showsAlpha = NO;
            panel.worksWhenModal = YES;
            panel.hidesOnDeactivate = NO;
            [panel makeKeyAndOrderFront:nil];
            [application runModalForWindow:panel];
            result = delegate.accepted ? 0 : 1;
            if (delegate.accepted) {
                NSColor* pickedColor = [panel.color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
                if (pickedColor) {
                    CGFloat redValue = 0.0;
                    CGFloat greenValue = 0.0;
                    CGFloat blueValue = 0.0;
                    CGFloat alpha = 1.0;
                    [pickedColor getRed:&redValue green:&greenValue blue:&blueValue alpha:&alpha];
                    *red = toonpixel::decode_unmanaged(static_cast<float>(redValue));
                    *green = toonpixel::decode_unmanaged(static_cast<float>(greenValue));
                    *blue = toonpixel::decode_unmanaged(static_cast<float>(blueValue));
                } else {
                    result = -1;
                }
            }
        } @catch (NSException* exception) {
            result = -1;
        } @finally {
            panel.delegate = previousDelegate;
            panel.accessoryView = previousAccessory;
            panel.title = previousTitle;
            panel.color = previousColor;
            panel.showsAlpha = previousAlpha;
            panel.worksWhenModal = previousWorksWhenModal;
            panel.hidesOnDeactivate = previousHidesOnDeactivate;
            [panel close];
        }
        return result;
    }
}
