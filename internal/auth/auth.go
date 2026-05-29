package auth

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
)

type File struct {
	AuthMode    string `json:"auth_mode"`
	OpenAIKey   any    `json:"OPENAI_API_KEY"`
	Tokens      Tokens `json:"tokens"`
	LastRefresh string `json:"last_refresh"`
}

type Tokens struct {
	IDToken      string `json:"id_token"`
	AccessToken string `json:"access_token"`
	RefreshToken string `json:"refresh_token"`
	AccountID    string `json:"account_id"`
}

type RedactedFile struct {
	AuthMode    string
	AccountID   string
	LastRefresh string
}

func Parse(data []byte) (File, error) {
	var file File
	if err := json.Unmarshal(data, &file); err != nil {
		return File{}, fmt.Errorf("parse auth json: %w", err)
	}
	if err := file.Validate(); err != nil {
		return File{}, err
	}
	return file, nil
}

func Load(path string) (File, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return File{}, fmt.Errorf("read auth file: %w", err)
	}
	return Parse(data)
}

func (f File) Validate() error {
	if f.Tokens.AccessToken == "" {
		return errors.New("auth tokens.access_token is required")
	}
	return nil
}

func (f File) AccountSuffix(length int) string {
	if length <= 0 || f.Tokens.AccountID == "" {
		return ""
	}
	if len(f.Tokens.AccountID) <= length {
		return f.Tokens.AccountID
	}
	return f.Tokens.AccountID[len(f.Tokens.AccountID)-length:]
}

func (f File) Redacted() RedactedFile {
	return RedactedFile{
		AuthMode:    f.AuthMode,
		AccountID:   f.Tokens.AccountID,
		LastRefresh: f.LastRefresh,
	}
}

func (r RedactedFile) String() string {
	return fmt.Sprintf("auth_mode=%s account_id=%s last_refresh=%s", r.AuthMode, r.AccountID, r.LastRefresh)
}
