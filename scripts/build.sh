#!/usr/bin/env sh
set -eu

cd "$(dirname "$0")/.."

go run ./cmd/generate-icons
CGO_ENABLED=0 go test ./...

if ! command -v gcc >/dev/null 2>&1; then
  echo "Fyne desktop builds require CGO and a C compiler. Install gcc or your platform C toolchain." >&2
  exit 1
fi

CGO_ENABLED=1 go build -buildvcs=false -trimpath -ldflags="-s -w -X github.com/fearofmissingout/codex-quota-dock/internal/version.Version=0.6.1" -o codex-quota-dock ./cmd/codex-quota-dock

if command -v strip >/dev/null 2>&1; then
  strip --strip-all codex-quota-dock || true
fi

echo "Built $(pwd)/codex-quota-dock"
