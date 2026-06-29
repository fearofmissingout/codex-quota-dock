package appicon

import (
	_ "embed"

	"fyne.io/fyne/v2"
)

//go:embed codex-quota-dock.png
var iconPNG []byte

func Resource() fyne.Resource {
	return fyne.NewStaticResource("codex-quota-dock.png", iconPNG)
}

func Bytes() []byte {
	out := make([]byte, len(iconPNG))
	copy(out, iconPNG)
	return out
}
