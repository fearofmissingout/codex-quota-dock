package quota

import (
	"encoding/json"
	"fmt"
)

func ParsePayload(data []byte) ([]Snapshot, error) {
	var payload statusPayload
	if err := json.Unmarshal(data, &payload); err != nil {
		return nil, fmt.Errorf("parse usage payload: %w", err)
	}

	reached := ""
	if payload.RateLimitReachedType != nil {
		reached = payload.RateLimitReachedType.Type
	}

	snapshots := []Snapshot{
		makeSnapshot("codex", "", payload.PlanType, payload.RateLimit, payload.Credits, reached),
	}
	for _, additional := range payload.AdditionalRateLimits {
		limitID := additional.MeteredFeature
		if limitID == "" {
			limitID = additional.LimitName
		}
		snapshots = append(snapshots, makeSnapshot(
			limitID,
			additional.LimitName,
			payload.PlanType,
			additional.RateLimit,
			nil,
			"",
		))
	}

	return snapshots, nil
}

func makeSnapshot(limitID, limitName, planType string, details *rateLimitDetails, credits *creditDetails, reached string) Snapshot {
	return Snapshot{
		LimitID:              limitID,
		LimitName:            limitName,
		PlanType:             planType,
		Primary:              mapWindow(details, true),
		Secondary:            mapWindow(details, false),
		Credits:              mapCredits(credits),
		RateLimitReachedType: reached,
	}
}

func mapWindow(details *rateLimitDetails, primary bool) *Window {
	if details == nil {
		return nil
	}
	var source *windowSnapshot
	if primary {
		source = details.PrimaryWindow
	} else {
		source = details.SecondaryWindow
	}
	if source == nil {
		return nil
	}
	return &Window{
		UsedPercent:   source.UsedPercent,
		WindowMinutes: secondsToMinutes(source.LimitWindowSecond),
		ResetAfterSec: source.ResetAfterSecond,
		ResetsAt:      source.ResetAt,
	}
}

func mapCredits(details *creditDetails) *Credits {
	if details == nil {
		return nil
	}
	return &Credits{
		HasCredits: details.HasCredits,
		Unlimited:  details.Unlimited,
		Balance:    details.Balance,
	}
}

func secondsToMinutes(seconds int) int {
	if seconds <= 0 {
		return 0
	}
	return (seconds + 59) / 60
}
