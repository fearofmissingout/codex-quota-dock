//go:build cgo

package main

import (
	"testing"

	"fyne.io/fyne/v2"
)

func TestWindowDragSurfaceStartsOnDrag(t *testing.T) {
	starts := 0
	surface := newWindowDragSurface(func() {
		starts++
	})

	surface.Dragged(&fyne.DragEvent{Dragged: fyne.NewDelta(1, 0)})

	if starts != 1 {
		t.Fatalf("starts = %d, want 1", starts)
	}
}

func TestWindowDragSurfaceIgnoresZeroDrag(t *testing.T) {
	starts := 0
	surface := newWindowDragSurface(func() {
		starts++
	})

	surface.Dragged(&fyne.DragEvent{Dragged: fyne.NewDelta(0, 0)})

	if starts != 0 {
		t.Fatalf("starts = %d, want 0", starts)
	}
}
