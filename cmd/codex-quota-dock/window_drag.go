//go:build cgo

package main

import (
	"image/color"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/widget"
)

type windowDragSurface struct {
	widget.BaseWidget
	onStart func()
}

func newWindowDragSurface(onStart func()) *windowDragSurface {
	surface := &windowDragSurface{onStart: onStart}
	surface.ExtendBaseWidget(surface)
	return surface
}

func (s *windowDragSurface) Dragged(ev *fyne.DragEvent) {
	if ev == nil || ev.Dragged.IsZero() || s.onStart == nil {
		return
	}
	s.onStart()
}

func (s *windowDragSurface) DragEnd() {}

func (s *windowDragSurface) MinSize() fyne.Size {
	return fyne.NewSize(1, 1)
}

func (s *windowDragSurface) CreateRenderer() fyne.WidgetRenderer {
	return widget.NewSimpleRenderer(canvas.NewRectangle(color.NRGBA{}))
}
