package settings

import "time"

func DefaultPollingInterval() time.Duration {
	return 5 * time.Minute
}

func PollingOptions() []time.Duration {
	return []time.Duration{0, time.Minute, 5 * time.Minute, 10 * time.Minute}
}

func DefaultQuotaAlertThreshold() int {
	return 20
}

func QuotaAlertThresholdOptions() []int {
	return []int{0, 5, 10, 20, 30}
}
