//go:build cgo

package main

import (
	"testing"

	"fyne.io/fyne/v2"
)

func TestDragDeltaPixelsUsesCanvasScale(t *testing.T) {
	x, y := dragDeltaPixels(fyne.NewDelta(4, -3), 1.5)

	if x != 6 || y != -5 {
		t.Fatalf("dragDeltaPixels() = (%d, %d), want (6, -5)", x, y)
	}
}

func TestDragDeltaPixelsDefaultsInvalidScale(t *testing.T) {
	x, y := dragDeltaPixels(fyne.NewDelta(2, 3), 0)

	if x != 2 || y != 3 {
		t.Fatalf("dragDeltaPixels() = (%d, %d), want (2, 3)", x, y)
	}
}
