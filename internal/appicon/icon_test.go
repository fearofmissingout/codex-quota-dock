package appicon_test

import (
	"bytes"
	"image/png"
	"testing"

	"github.com/fearofmissingout/codex-quota-dock/internal/appicon"
)

func TestResourceContainsPNGIcon(t *testing.T) {
	resource := appicon.Resource()
	if resource.Name() != "codex-quota-dock.png" {
		t.Fatalf("Name=%q want codex-quota-dock.png", resource.Name())
	}
	if len(resource.Content()) == 0 {
		t.Fatal("icon resource content is empty")
	}
	if _, err := png.Decode(bytes.NewReader(resource.Content())); err != nil {
		t.Fatalf("decode icon png: %v", err)
	}
}

func TestBytesReturnsCopy(t *testing.T) {
	first := appicon.Bytes()
	second := appicon.Bytes()
	if len(first) == 0 || len(second) == 0 {
		t.Fatal("icon bytes are empty")
	}
	first[0] ^= 0xff
	if first[0] == second[0] {
		t.Fatal("Bytes returned shared mutable slice")
	}
}
