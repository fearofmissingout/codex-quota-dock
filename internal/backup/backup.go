package backup

import (
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/fearofmissingout/codex-quota-dock/internal/profile"
)

const Version = 1

type Document struct {
	Version    int       `json:"version"`
	ExportedAt time.Time `json:"exported_at"`
	Settings   Settings  `json:"settings"`
	Profiles   []Profile `json:"profiles"`
}

type Settings struct {
	PollingInterval          string `json:"polling_interval,omitempty"`
	FiveHourAlertThreshold   int    `json:"five_hour_alert_threshold,omitempty"`
	WeeklyAlertThreshold     int    `json:"weekly_alert_threshold,omitempty"`
	AutoRestartAfterSwitch   bool   `json:"auto_restart_after_switch,omitempty"`
	ShowRestartReminder      bool   `json:"show_restart_reminder,omitempty"`
	CheckForUpdatesOnStartup bool   `json:"check_for_updates_on_startup,omitempty"`
	StartAtLogin             bool   `json:"start_at_login,omitempty"`
}

type Profile struct {
	Alias         string          `json:"alias"`
	Pinned        bool            `json:"pinned,omitempty"`
	AccountID     string          `json:"account_id,omitempty"`
	AccountSuffix string          `json:"account_suffix,omitempty"`
	AuthMode      string          `json:"auth_mode,omitempty"`
	AuthJSON      json.RawMessage `json:"auth_json"`
}

type ImportSummary struct {
	Created  int
	Updated  int
	Skipped  int
	Settings Settings
}

func Export(store *profile.Store, settings Settings) ([]byte, error) {
	doc := Document{
		Version:    Version,
		ExportedAt: time.Now().UTC(),
		Settings:   settings,
	}
	for _, prof := range store.Profiles() {
		authJSON, err := store.ReadAuth(prof.ID)
		if err != nil {
			return nil, fmt.Errorf("read auth for %q: %w", prof.Alias, err)
		}
		doc.Profiles = append(doc.Profiles, Profile{
			Alias:         prof.Alias,
			Pinned:        prof.Pinned,
			AccountID:     prof.AccountID,
			AccountSuffix: prof.AccountSuffix,
			AuthMode:      prof.AuthMode,
			AuthJSON:      json.RawMessage(authJSON),
		})
	}
	data, err := json.MarshalIndent(doc, "", "  ")
	if err != nil {
		return nil, fmt.Errorf("encode backup: %w", err)
	}
	return data, nil
}

func Import(store *profile.Store, data []byte) (ImportSummary, error) {
	var doc Document
	if err := json.Unmarshal(data, &doc); err != nil {
		return ImportSummary{}, fmt.Errorf("parse backup: %w", err)
	}
	if doc.Version != Version {
		return ImportSummary{}, fmt.Errorf("unsupported backup version %d", doc.Version)
	}
	summary := ImportSummary{Settings: doc.Settings}
	for _, item := range doc.Profiles {
		authJSON := []byte(item.AuthJSON)
		if len(authJSON) == 0 || strings.TrimSpace(string(authJSON)) == "" {
			return summary, errors.New("backup profile is missing auth_json")
		}
		alias := strings.TrimSpace(item.Alias)
		if alias == "" {
			var err error
			alias, err = store.SuggestAlias("profile", authJSON)
			if err != nil {
				return summary, err
			}
		}
		alias = uniqueAlias(store, alias, item.AccountID)
		updated, existed, err := store.UpdateByAccountID(alias, authJSON)
		if err != nil {
			return summary, err
		}
		if existed {
			if _, err := store.SetPinned(updated.ID, item.Pinned); err != nil {
				return summary, err
			}
			summary.Updated++
			continue
		}
		created, err := store.ImportBytes(alias, authJSON)
		if err != nil {
			return summary, err
		}
		if item.Pinned {
			if _, err := store.SetPinned(created.ID, true); err != nil {
				return summary, err
			}
		}
		summary.Created++
	}
	return summary, nil
}

func uniqueAlias(store *profile.Store, alias, sameAccountID string) string {
	if existing, ok := store.FindByAlias(alias); !ok || existing.AccountID == sameAccountID {
		return alias
	}
	base := alias
	for i := 2; ; i++ {
		next := fmt.Sprintf("%s-%d", base, i)
		if _, ok := store.FindByAlias(next); !ok {
			return next
		}
	}
}
