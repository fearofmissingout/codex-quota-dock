//go:build linux && cgo

package main

/*
#cgo LDFLAGS: -lX11

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>

static int codexStartX11WindowMove(unsigned long windowHandle) {
	Display *display = XOpenDisplay(NULL);
	if (display == NULL) {
		return 0;
	}

	Window root = DefaultRootWindow(display);
	Window rootReturn;
	Window childReturn;
	int rootX = 0;
	int rootY = 0;
	int winX = 0;
	int winY = 0;
	unsigned int mask = 0;
	XQueryPointer(display, root, &rootReturn, &childReturn, &rootX, &rootY, &winX, &winY, &mask);

	Atom moveresize = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
	XEvent event;
	memset(&event, 0, sizeof(event));
	event.xclient.type = ClientMessage;
	event.xclient.display = display;
	event.xclient.window = (Window)windowHandle;
	event.xclient.message_type = moveresize;
	event.xclient.format = 32;
	event.xclient.data.l[0] = rootX;
	event.xclient.data.l[1] = rootY;
	event.xclient.data.l[2] = 8;
	event.xclient.data.l[3] = 1;
	event.xclient.data.l[4] = 1;

	XUngrabPointer(display, CurrentTime);
	int sent = XSendEvent(display, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
	XFlush(display);
	XCloseDisplay(display);
	return sent;
}
*/
import "C"

import (
	"os"

	"fyne.io/fyne/v2"
	fynedriver "fyne.io/fyne/v2/driver"
)

func supportsBorderlessMonitorDrag() bool {
	return os.Getenv("DISPLAY") != ""
}

func configureBorderlessMonitorDrag(_ fyne.Window) {}

func startSystemWindowDrag(win fyne.Window) {
	if win == nil {
		return
	}
	native, ok := win.(fynedriver.NativeWindow)
	if !ok {
		return
	}
	native.RunNative(func(context any) {
		winContext, ok := context.(fynedriver.X11WindowContext)
		if !ok || winContext.WindowHandle == 0 {
			return
		}
		C.codexStartX11WindowMove(C.ulong(winContext.WindowHandle))
	})
}
