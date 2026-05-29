package localusage

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"time"
)

const switchHistoryFile = "auth_switch_history.json"

type switchHistory struct {
	Switches []switchHistoryEntry `json:"switches"`
}

type switchHistoryEntry struct {
	At        time.Time `json:"at"`
	ProfileID string    `json:"profile_id"`
	AccountID string    `json:"account_id,omitempty"`
	Alias     string    `json:"alias,omitempty"`
}

func LoadSwitchHistory(root string) ([]SwitchAttribution, error) {
	path := filepath.Join(root, switchHistoryFile)
	data, err := os.ReadFile(path)
	if os.IsNotExist(err) {
		return nil, nil
	}
	if err != nil {
		return nil, fmt.Errorf("read switch history: %w", err)
	}
	var history switchHistory
	if err := json.Unmarshal(data, &history); err != nil {
		return nil, fmt.Errorf("parse switch history: %w", err)
	}
	out := make([]SwitchAttribution, 0, len(history.Switches))
	for _, item := range history.Switches {
		out = append(out, SwitchAttribution{
			At:        item.At,
			ProfileID: item.ProfileID,
			AccountID: item.AccountID,
			Alias:     item.Alias,
		})
	}
	sortSwitches(out)
	return out, nil
}

func RecordSwitch(root string, item SwitchAttribution) error {
	if item.At.IsZero() {
		item.At = time.Now()
	}
	history, err := LoadSwitchHistory(root)
	if err != nil {
		return err
	}
	history = append(history, item)
	sortSwitches(history)
	encoded := switchHistory{Switches: make([]switchHistoryEntry, 0, len(history))}
	for _, entry := range history {
		encoded.Switches = append(encoded.Switches, switchHistoryEntry{
			At:        entry.At,
			ProfileID: entry.ProfileID,
			AccountID: entry.AccountID,
			Alias:     entry.Alias,
		})
	}
	data, err := json.MarshalIndent(encoded, "", "  ")
	if err != nil {
		return fmt.Errorf("encode switch history: %w", err)
	}
	if err := os.MkdirAll(root, 0o700); err != nil {
		return fmt.Errorf("create switch history dir: %w", err)
	}
	path := filepath.Join(root, switchHistoryFile)
	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, data, 0o600); err != nil {
		return fmt.Errorf("write switch history: %w", err)
	}
	if err := os.Rename(tmp, path); err != nil {
		_ = os.Remove(tmp)
		return fmt.Errorf("replace switch history: %w", err)
	}
	return nil
}

func sortSwitches(items []SwitchAttribution) {
	sort.Slice(items, func(i, j int) bool {
		return items[i].At.Before(items[j].At)
	})
}
