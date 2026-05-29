//go:build cgo

package main

import (
	"image/color"
	"math"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/driver/desktop"
	"fyne.io/fyne/v2/widget"
)

type windowDragHandle struct {
	widget.BaseWidget
	onDrag func(fyne.Delta)
}

func newWindowDragHandle(onDrag func(fyne.Delta)) *windowDragHandle {
	handle := &windowDragHandle{onDrag: onDrag}
	handle.ExtendBaseWidget(handle)
	return handle
}

func (h *windowDragHandle) Dragged(ev *fyne.DragEvent) {
	if ev == nil || ev.Dragged.IsZero() || h.onDrag == nil {
		return
	}
	h.onDrag(ev.Dragged)
}

func (h *windowDragHandle) DragEnd() {}

func (h *windowDragHandle) Cursor() desktop.Cursor {
	return desktop.PointerCursor
}

func (h *windowDragHandle) MinSize() fyne.Size {
	return fyne.NewSize(1, 20)
}

func (h *windowDragHandle) CreateRenderer() fyne.WidgetRenderer {
	return widget.NewSimpleRenderer(canvas.NewRectangle(color.NRGBA{}))
}

func dragDeltaPixels(delta fyne.Delta, scale float32) (int, int) {
	if scale <= 0 {
		scale = 1
	}
	return int(math.Round(float64(delta.DX * scale))), int(math.Round(float64(delta.DY * scale)))
}
