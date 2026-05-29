package localusage

import (
	"os"
	"path/filepath"
	"testing"
	"time"
)

func TestScanUsesLastTokenUsageAsIncrement(t *testing.T) {
	root := t.TempDir()
	sessionPath := filepath.Join(root, "sessions", "2026", "05", "29", "rollout-test.jsonl")
	writeFile(t, sessionPath, `{"timestamp":"2026-05-29T01:00:00Z","type":"session_meta","payload":{"id":"session-a","cwd":"C:\\repo"}}
{"timestamp":"2026-05-29T01:01:00Z","type":"event_msg","payload":{"type":"token_count","info":{"total_token_usage":{"input_tokens":1000,"cached_input_tokens":100,"output_tokens":20,"reasoning_output_tokens":5,"total_tokens":1020},"last_token_usage":{"input_tokens":1000,"cached_input_tokens":100,"output_tokens":20,"reasoning_output_tokens":5,"total_tokens":1020}}}}
{"timestamp":"2026-05-29T01:02:00Z","type":"event_msg","payload":{"type":"token_count","info":{"total_token_usage":{"input_tokens":3000,"cached_input_tokens":300,"output_tokens":70,"reasoning_output_tokens":10,"total_tokens":3070},"last_token_usage":{"input_tokens":2000,"cached_input_tokens":200,"output_tokens":50,"reasoning_output_tokens":5,"total_tokens":2050}}}}
`)

	summary, err := Scan(root, Options{Now: mustTime("2026-05-29T12:00:00Z")})
	if err != nil {
		t.Fatal(err)
	}

	if summary.Total.Total != 3070 {
		t.Fatalf("total=%d want 3070 from summed last_token_usage", summary.Total.Total)
	}
	if summary.Total.Input != 3000 || summary.Total.CachedInput != 300 || summary.Total.Output != 70 || summary.Total.ReasoningOutput != 10 {
		t.Fatalf("usage=%+v", summary.Total)
	}
	if len(summary.Sessions) != 1 || summary.Sessions[0].ID != "session-a" {
		t.Fatalf("sessions=%+v", summary.Sessions)
	}
}

func TestScanIncludesArchivedSessionsAndSkipsMalformedLines(t *testing.T) {
	root := t.TempDir()
	writeFile(t, filepath.Join(root, "archived_sessions", "rollout-archived.jsonl"), `{"timestamp":"2026-05-28T10:00:00Z","type":"session_meta","payload":{"id":"archived"}}
not-json
{"timestamp":"2026-05-28T10:01:00Z","type":"event_msg","payload":{"type":"token_count","info":{"last_token_usage":{"input_tokens":10,"cached_input_tokens":2,"output_tokens":3,"reasoning_output_tokens":1,"total_tokens":13}}}}
`)

	summary, err := Scan(root, Options{Now: mustTime("2026-05-29T12:00:00Z")})
	if err != nil {
		t.Fatal(err)
	}

	if summary.Total.Total != 13 {
		t.Fatalf("total=%d want 13", summary.Total.Total)
	}
	if summary.ParseErrors != 1 {
		t.Fatalf("parseErrors=%d want 1", summary.ParseErrors)
	}
}

func TestScanAttributesEventsBySwitchHistory(t *testing.T) {
	root := t.TempDir()
	writeFile(t, filepath.Join(root, "sessions", "2026", "05", "29", "rollout-switch.jsonl"), `{"timestamp":"2026-05-29T08:59:00Z","type":"event_msg","payload":{"type":"token_count","info":{"last_token_usage":{"input_tokens":1,"output_tokens":1,"total_tokens":2}}}}
{"timestamp":"2026-05-29T09:10:00Z","type":"event_msg","payload":{"type":"token_count","info":{"last_token_usage":{"input_tokens":10,"output_tokens":5,"total_tokens":15}}}}
{"timestamp":"2026-05-29T11:10:00Z","type":"event_msg","payload":{"type":"token_count","info":{"last_token_usage":{"input_tokens":20,"output_tokens":5,"total_tokens":25}}}}
`)

	summary, err := Scan(root, Options{
		Now: mustTime("2026-05-29T12:00:00Z"),
		Switches: []SwitchAttribution{
			{At: mustTime("2026-05-29T09:00:00Z"), ProfileID: "company", Alias: "company"},
			{At: mustTime("2026-05-29T11:00:00Z"), ProfileID: "pro", Alias: "pro"},
		},
	})
	if err != nil {
		t.Fatal(err)
	}

	if summary.ByProfile[""].Total != 2 {
		t.Fatalf("unknown=%+v want 2 total", summary.ByProfile[""])
	}
	if summary.ByProfile["company"].Total != 15 {
		t.Fatalf("company=%+v want 15 total", summary.ByProfile["company"])
	}
	if summary.ByProfile["pro"].Total != 25 {
		t.Fatalf("pro=%+v want 25 total", summary.ByProfile["pro"])
	}
}

func writeFile(t *testing.T, path, content string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatal(err)
	}
}

func mustTime(value string) time.Time {
	t, err := time.Parse(time.RFC3339, value)
	if err != nil {
		panic(err)
	}
	return t
}
