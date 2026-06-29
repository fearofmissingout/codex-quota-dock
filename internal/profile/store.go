package profile

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/fearofmissingout/codex-quota-dock/internal/auth"
)

type Profile struct {
	ID            string    `json:"id"`
	Alias         string    `json:"alias"`
	AccountID     string    `json:"account_id"`
	AccountSuffix string    `json:"account_suffix"`
	AuthMode      string    `json:"auth_mode"`
	Pinned        bool      `json:"pinned,omitempty"`
	LastRefresh   string    `json:"last_refresh"`
	CreatedAt     time.Time `json:"created_at"`
}

type Store struct {
	root     string
	profiles []Profile
}

type metadataFile struct {
	Profiles []Profile `json:"profiles"`
}

func Open(root string) (*Store, error) {
	store := &Store{root: root}
	if err := os.MkdirAll(filepath.Join(root, "profiles"), 0o700); err != nil {
		return nil, fmt.Errorf("create profile store: %w", err)
	}
	if err := os.MkdirAll(filepath.Join(root, "backups"), 0o700); err != nil {
		return nil, fmt.Errorf("create backup store: %w", err)
	}
	if err := store.load(); err != nil {
		return nil, err
	}
	return store, nil
}

func (s *Store) Profiles() []Profile {
	out := make([]Profile, len(s.profiles))
	copy(out, s.profiles)
	return out
}

func (s *Store) AuthPath(profileID string) string {
	return filepath.Join(s.root, "profiles", profileID, "auth.json")
}

func (s *Store) ReadAuth(profileID string) ([]byte, error) {
	data, err := os.ReadFile(s.AuthPath(profileID))
	if err != nil {
		return nil, fmt.Errorf("read profile auth: %w", err)
	}
	return data, nil
}

func (s *Store) BackupsDir() string {
	return filepath.Join(s.root, "backups")
}

func (s *Store) Import(alias, sourcePath string) (Profile, error) {
	data, err := os.ReadFile(sourcePath)
	if err != nil {
		return Profile{}, fmt.Errorf("read source auth: %w", err)
	}
	return s.ImportBytes(alias, data)
}

func (s *Store) ImportBytes(alias string, authJSON []byte) (Profile, error) {
	alias = strings.TrimSpace(alias)
	if alias == "" {
		return Profile{}, errors.New("profile alias is required")
	}
	for _, existing := range s.profiles {
		if strings.EqualFold(existing.Alias, alias) {
			return Profile{}, fmt.Errorf("profile alias %q already exists", alias)
		}
	}

	file, err := auth.Parse(authJSON)
	if err != nil {
		return Profile{}, err
	}
	id, err := randomID()
	if err != nil {
		return Profile{}, err
	}
	prof := Profile{
		ID:            id,
		Alias:         alias,
		AccountID:     file.Tokens.AccountID,
		AccountSuffix: file.AccountSuffix(6),
		AuthMode:      file.AuthMode,
		LastRefresh:   file.LastRefresh,
		CreatedAt:     time.Now().UTC(),
	}

	destDir := filepath.Join(s.root, "profiles", id)
	if err := os.MkdirAll(destDir, 0o700); err != nil {
		return Profile{}, fmt.Errorf("create profile dir: %w", err)
	}
	if err := writeFileAtomic(filepath.Join(destDir, "auth.json"), authJSON, 0o600); err != nil {
		return Profile{}, fmt.Errorf("write profile auth: %w", err)
	}

	s.profiles = append(s.profiles, prof)
	if err := s.save(); err != nil {
		return Profile{}, err
	}
	return prof, nil
}

func (s *Store) SuggestAlias(prefix string, authJSON []byte) (string, error) {
	prefix = strings.TrimSpace(prefix)
	if prefix == "" {
		prefix = "profile"
	}
	file, err := auth.Parse(authJSON)
	if err != nil {
		return "", err
	}
	base := prefix
	if suffix := file.AccountSuffix(6); suffix != "" {
		base = fmt.Sprintf("%s-%s", prefix, suffix)
	}
	alias := base
	for i := 2; s.aliasExists(alias, ""); i++ {
		alias = fmt.Sprintf("%s-%d", base, i)
	}
	return alias, nil
}

func (s *Store) UpdateByAccountID(alias string, authJSON []byte) (Profile, bool, error) {
	file, err := auth.Parse(authJSON)
	if err != nil {
		return Profile{}, false, err
	}
	for _, existing := range s.profiles {
		if existing.AccountID == file.Tokens.AccountID {
			updated, err := s.Update(existing.ID, alias, authJSON)
			return updated, true, err
		}
	}
	return Profile{}, false, nil
}

func (s *Store) Update(profileID, alias string, authJSON []byte) (Profile, error) {
	alias = strings.TrimSpace(alias)
	if alias == "" {
		return Profile{}, errors.New("profile alias is required")
	}
	index := -1
	for i, existing := range s.profiles {
		if existing.ID == profileID {
			index = i
			continue
		}
		if strings.EqualFold(existing.Alias, alias) {
			return Profile{}, fmt.Errorf("profile alias %q already exists", alias)
		}
	}
	if index < 0 {
		return Profile{}, fmt.Errorf("profile %q not found", profileID)
	}

	file, err := auth.Parse(authJSON)
	if err != nil {
		return Profile{}, err
	}
	dest := s.AuthPath(profileID)
	if err := writeFileAtomic(dest, authJSON, 0o600); err != nil {
		return Profile{}, fmt.Errorf("write profile auth: %w", err)
	}

	updated := s.profiles[index]
	updated.Alias = alias
	updated.AccountID = file.Tokens.AccountID
	updated.AccountSuffix = file.AccountSuffix(6)
	updated.AuthMode = file.AuthMode
	updated.LastRefresh = file.LastRefresh
	s.profiles[index] = updated
	if err := s.save(); err != nil {
		return Profile{}, err
	}
	return updated, nil
}

func (s *Store) FindByAccountID(accountID string) (Profile, bool) {
	for _, prof := range s.profiles {
		if prof.AccountID == accountID {
			return prof, true
		}
	}
	return Profile{}, false
}

func (s *Store) FindByAlias(alias string) (Profile, bool) {
	for _, prof := range s.profiles {
		if strings.EqualFold(prof.Alias, alias) {
			return prof, true
		}
	}
	return Profile{}, false
}

func (s *Store) SetPinned(profileID string, pinned bool) (Profile, error) {
	for i := range s.profiles {
		if s.profiles[i].ID == profileID {
			s.profiles[i].Pinned = pinned
			if err := s.save(); err != nil {
				return Profile{}, err
			}
			return s.profiles[i], nil
		}
	}
	return Profile{}, fmt.Errorf("profile %q not found", profileID)
}

func (s *Store) Delete(profileID string) error {
	index := -1
	for i := range s.profiles {
		if s.profiles[i].ID == profileID {
			index = i
			break
		}
	}
	if index < 0 {
		return fmt.Errorf("profile %q not found", profileID)
	}

	profileDir := filepath.Join(s.root, "profiles", profileID)
	if err := os.RemoveAll(profileDir); err != nil {
		return fmt.Errorf("delete profile auth: %w", err)
	}
	s.profiles = append(s.profiles[:index], s.profiles[index+1:]...)
	if err := s.save(); err != nil {
		return err
	}
	return nil
}

func (s *Store) load() error {
	data, err := os.ReadFile(s.metadataPath())
	if errors.Is(err, os.ErrNotExist) {
		s.profiles = nil
		return nil
	}
	if err != nil {
		return fmt.Errorf("read profiles metadata: %w", err)
	}
	var metadata metadataFile
	if err := json.Unmarshal(data, &metadata); err != nil {
		return fmt.Errorf("parse profiles metadata: %w", err)
	}
	s.profiles = metadata.Profiles
	return nil
}

func (s *Store) save() error {
	data, err := json.MarshalIndent(metadataFile{Profiles: s.profiles}, "", "  ")
	if err != nil {
		return fmt.Errorf("encode profiles metadata: %w", err)
	}
	tmp := s.metadataPath() + ".tmp"
	if err := os.WriteFile(tmp, data, 0o600); err != nil {
		return fmt.Errorf("write profiles metadata: %w", err)
	}
	if err := os.Rename(tmp, s.metadataPath()); err != nil {
		return fmt.Errorf("replace profiles metadata: %w", err)
	}
	return nil
}

func (s *Store) metadataPath() string {
	return filepath.Join(s.root, "profiles.json")
}

func (s *Store) aliasExists(alias, exceptProfileID string) bool {
	for _, existing := range s.profiles {
		if existing.ID != exceptProfileID && strings.EqualFold(existing.Alias, alias) {
			return true
		}
	}
	return false
}

func writeFileAtomic(path string, data []byte, mode os.FileMode) error {
	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, data, mode); err != nil {
		return err
	}
	if err := os.Rename(tmp, path); err != nil {
		_ = os.Remove(tmp)
		return err
	}
	return nil
}

func randomID() (string, error) {
	var buf [8]byte
	if _, err := rand.Read(buf[:]); err != nil {
		return "", fmt.Errorf("generate profile id: %w", err)
	}
	return hex.EncodeToString(buf[:]), nil
}
