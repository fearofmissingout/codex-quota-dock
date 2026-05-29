package main

import (
	"testing"
	"time"
)

func TestMonitorClickActionOpensOnSecondClickWithinThreshold(t *testing.T) {
	first := time.Date(2026, 5, 29, 10, 0, 0, 0, time.UTC)
	open, next := monitorClickAction(time.Time{}, first)
	if open {
		t.Fatal("first click should not open details")
	}
	if !next.Equal(first) {
		t.Fatalf("next=%v want %v", next, first)
	}

	open, next = monitorClickAction(next, first.Add(250*time.Millisecond))
	if !open {
		t.Fatal("second click within threshold should open details")
	}
	if !next.IsZero() {
		t.Fatalf("next=%v want zero after opening", next)
	}
}

func TestMonitorClickActionDoesNotOpenAfterThreshold(t *testing.T) {
	first := time.Date(2026, 5, 29, 10, 0, 0, 0, time.UTC)
	open, next := monitorClickAction(first, first.Add(600*time.Millisecond))
	if open {
		t.Fatal("slow second click should not open details")
	}
	if !next.Equal(first.Add(600 * time.Millisecond)) {
		t.Fatalf("next=%v want slow click time", next)
	}
}
