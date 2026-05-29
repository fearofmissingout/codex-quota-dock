package profile

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
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

func (s *Store) BackupsDir() string {
	return filepath.Join(s.root, "backups")
}

func (s *Store) Import(alias, sourcePath string) (Profile, error) {
	alias = strings.TrimSpace(alias)
	if alias == "" {
		return Profile{}, errors.New("profile alias is required")
	}
	for _, existing := range s.profiles {
		if strings.EqualFold(existing.Alias, alias) {
			return Profile{}, fmt.Errorf("profile alias %q already exists", alias)
		}
	}

	file, err := auth.Load(sourcePath)
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
	if err := copyFile(sourcePath, filepath.Join(destDir, "auth.json"), 0o600); err != nil {
		return Profile{}, err
	}

	s.profiles = append(s.profiles, prof)
	if err := s.save(); err != nil {
		return Profile{}, err
	}
	return prof, nil
}

func (s *Store) FindByAccountID(accountID string) (Profile, bool) {
	for _, prof := range s.profiles {
		if prof.AccountID == accountID {
			return prof, true
		}
	}
	return Profile{}, false
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

func randomID() (string, error) {
	var buf [8]byte
	if _, err := rand.Read(buf[:]); err != nil {
		return "", fmt.Errorf("generate profile id: %w", err)
	}
	return hex.EncodeToString(buf[:]), nil
}

func copyFile(source, dest string, mode os.FileMode) error {
	in, err := os.Open(source)
	if err != nil {
		return fmt.Errorf("open source auth: %w", err)
	}
	defer in.Close()

	out, err := os.OpenFile(dest, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, mode)
	if err != nil {
		return fmt.Errorf("open destination auth: %w", err)
	}
	if _, err := io.Copy(out, in); err != nil {
		_ = out.Close()
		return fmt.Errorf("copy auth: %w", err)
	}
	if err := out.Close(); err != nil {
		return fmt.Errorf("close destination auth: %w", err)
	}
	return nil
}
