package quota

import "math"

func WindowLabel(minutes int) string {
	switch {
	case approximate(minutes, 5*60):
		return "5h"
	case approximate(minutes, 24*60):
		return "daily"
	case approximate(minutes, 7*24*60):
		return "weekly"
	case approximate(minutes, 30*24*60):
		return "monthly"
	case approximate(minutes, 365*24*60):
		return "annual"
	default:
		return "usage"
	}
}

func RemainingPercent(used float64) float64 {
	remaining := 100 - used
	if remaining < 0 {
		return 0
	}
	if remaining > 100 {
		return 100
	}
	return remaining
}

func approximate(actual, expected int) bool {
	if actual <= 0 || expected <= 0 {
		return false
	}
	ratio := float64(actual) / float64(expected)
	return math.Abs(1-ratio) <= 0.05
}
