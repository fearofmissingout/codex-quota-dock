package quota

import (
	"context"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"codex-quota-monitor/internal/auth"
)

var (
	ErrUnauthorized    = errors.New("auth expired or unauthorized")
	ErrRateLimited     = errors.New("rate limited while checking quota")
	ErrInvalidResponse = errors.New("unexpected usage response")
)

type Client struct {
	baseURL    string
	path       string
	httpClient *http.Client
}

func NewClient(baseURL, path string) Client {
	return Client{
		baseURL: strings.TrimRight(baseURL, "/"),
		path:    "/" + strings.TrimLeft(path, "/"),
		httpClient: &http.Client{
			Timeout: 20 * time.Second,
		},
	}
}

func DefaultClient() Client {
	return NewClient("https://chatgpt.com/backend-api", "/wham/usage")
}

func (c Client) Fetch(ctx context.Context, file auth.File) ([]Snapshot, error) {
	if err := file.Validate(); err != nil {
		return nil, err
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, c.baseURL+c.path, nil)
	if err != nil {
		return nil, fmt.Errorf("build quota request: %w", err)
	}
	req.Header.Set("Authorization", "Bearer "+file.Tokens.AccessToken)
	if file.Tokens.AccountID != "" {
		req.Header.Set("ChatGPT-Account-Id", file.Tokens.AccountID)
	}
	req.Header.Set("User-Agent", "codex-quota-monitor")

	res, err := c.httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("fetch quota: %w", err)
	}
	defer res.Body.Close()

	body, err := io.ReadAll(res.Body)
	if err != nil {
		return nil, fmt.Errorf("read quota response: %w", err)
	}

	switch res.StatusCode {
	case http.StatusOK:
		snapshots, err := ParsePayload(body)
		if err != nil {
			return nil, fmt.Errorf("%w: %v", ErrInvalidResponse, err)
		}
		return snapshots, nil
	case http.StatusUnauthorized, http.StatusForbidden:
		return nil, fmt.Errorf("%w: http %d", ErrUnauthorized, res.StatusCode)
	case http.StatusTooManyRequests:
		return nil, fmt.Errorf("%w: http %d", ErrRateLimited, res.StatusCode)
	default:
		return nil, fmt.Errorf("quota request failed: http %d: %s", res.StatusCode, strings.TrimSpace(string(body)))
	}
}
