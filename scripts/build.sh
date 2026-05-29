#!/usr/bin/env sh
set -eu

cd "$(dirname "$0")/.."

go test ./...

if ! command -v gcc >/dev/null 2>&1; then
  echo "Fyne desktop builds require CGO and a C compiler. Install gcc or your platform C toolchain." >&2
  exit 1
fi

CGO_ENABLED=1 go build -o codex-quota-dock ./cmd/codex-quota-dock

echo "Built $(pwd)/codex-quota-dock"
