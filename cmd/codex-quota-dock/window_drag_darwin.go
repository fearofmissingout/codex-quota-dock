//go:build darwin && cgo

package main

/*
#cgo CFLAGS: -x objective-c
#cgo LDFLAGS: -framework Cocoa

#import <Cocoa/Cocoa.h>

static void codexSetMovableByBackground(void *nsWindow) {
	if (nsWindow == nil) {
		return;
	}
	NSWindow *window = (__bridge NSWindow *)nsWindow;
	[window setMovableByWindowBackground:YES];
}

static void codexStartWindowDrag(void *nsWindow) {
	if (nsWindow == nil) {
		return;
	}
	NSWindow *window = (__bridge NSWindow *)nsWindow;
	NSEvent *event = [NSApp currentEvent];
	if (event != nil && [window respondsToSelector:@selector(performWindowDragWithEvent:)]) {
		[window performWindowDragWithEvent:event];
	}
}
*/
import "C"

import (
	"unsafe"

	"fyne.io/fyne/v2"
	fynedriver "fyne.io/fyne/v2/driver"
)

func supportsBorderlessMonitorDrag() bool {
	return true
}

func configureBorderlessMonitorDrag(win fyne.Window) {
	withMacWindow(win, func(nsWindow uintptr) {
		C.codexSetMovableByBackground(unsafe.Pointer(nsWindow))
	})
}

func startSystemWindowDrag(win fyne.Window) {
	withMacWindow(win, func(nsWindow uintptr) {
		C.codexStartWindowDrag(unsafe.Pointer(nsWindow))
	})
}

func withMacWindow(win fyne.Window, fn func(uintptr)) {
	if win == nil || fn == nil {
		return
	}
	native, ok := win.(fynedriver.NativeWindow)
	if !ok {
		return
	}
	native.RunNative(func(context any) {
		winContext, ok := context.(fynedriver.MacWindowContext)
		if !ok || winContext.NSWindow == 0 {
			return
		}
		fn(winContext.NSWindow)
	})
}
