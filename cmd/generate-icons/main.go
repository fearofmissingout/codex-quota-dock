package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"image"
	"image/color"
	"image/png"
	"math"
	"os"
	"path/filepath"
)

var (
	transparent = color.NRGBA{}
	shadow      = color.NRGBA{R: 8, G: 14, B: 20, A: 255}
	base        = color.NRGBA{R: 16, G: 24, B: 32, A: 255}
	panel       = color.NRGBA{R: 23, G: 34, B: 45, A: 255}
	white       = color.NRGBA{R: 234, G: 247, B: 244, A: 255}
	teal        = color.NRGBA{R: 53, G: 214, B: 180, A: 255}
)

func main() {
	if err := run(); err != nil {
		fmt.Fprintf(os.Stderr, "generate icons: %v\n", err)
		os.Exit(1)
	}
}

func run() error {
	repoRoot, err := findRepoRoot()
	if err != nil {
		return err
	}
	assetDir := filepath.Join(repoRoot, "assets", "icon")
	embedDir := filepath.Join(repoRoot, "internal", "appicon")
	if err := os.MkdirAll(assetDir, 0755); err != nil {
		return fmt.Errorf("create icon asset dir: %w", err)
	}
	if err := os.MkdirAll(embedDir, 0755); err != nil {
		return fmt.Errorf("create app icon embed dir: %w", err)
	}

	pngData, err := pngBytes(drawIcon(512))
	if err != nil {
		return err
	}
	for _, path := range []string{
		filepath.Join(assetDir, "codex-quota-dock.png"),
		filepath.Join(embedDir, "codex-quota-dock.png"),
	} {
		if err := os.WriteFile(path, pngData, 0644); err != nil {
			return fmt.Errorf("write %s: %w", path, err)
		}
	}
	icoData, err := icoBytes([]int{16, 24, 32, 48, 64, 128, 256})
	if err != nil {
		return err
	}
	if err := os.WriteFile(filepath.Join(assetDir, "codex-quota-dock.ico"), icoData, 0644); err != nil {
		return fmt.Errorf("write ico: %w", err)
	}
	return nil
}

func findRepoRoot() (string, error) {
	dir, err := os.Getwd()
	if err != nil {
		return "", err
	}
	for {
		if _, err := os.Stat(filepath.Join(dir, "go.mod")); err == nil {
			return dir, nil
		}
		next := filepath.Dir(dir)
		if next == dir {
			return "", fmt.Errorf("go.mod not found from %s", dir)
		}
		dir = next
	}
}

func drawIcon(size int) image.Image {
	scale := 4
	large := image.NewNRGBA(image.Rect(0, 0, size*scale, size*scale))
	for y := 0; y < large.Bounds().Dy(); y++ {
		for x := 0; x < large.Bounds().Dx(); x++ {
			fx := float64(x) / float64(scale)
			fy := float64(y) / float64(scale)
			large.SetNRGBA(x, y, iconColorAt(fx, fy))
		}
	}
	return downsample(large, size, scale)
}

func iconColorAt(x, y float64) color.NRGBA {
	switch {
	case inRoundedRect(x, y, 30, 30, 452, 452, 115):
		c := base
		if inRoundedRect(x, y, 64, 64, 384, 384, 92) {
			c = panel
		}
		if onArc(x, y, 256, 256, 150, 54, 44, 316) {
			c = white
		}
		if onArc(x, y, 256, 256, 160, 34, -46, 20) {
			c = teal
		}
		if onLine(x, y, 166, 225, 274, 225, 30) {
			c = white
		}
		if onLine(x, y, 166, 280, 320, 280, 30) {
			c = teal
		}
		return c
	case inRoundedRect(x, y, 28, 36, 452, 452, 116):
		return color.NRGBA{R: shadow.R, G: shadow.G, B: shadow.B, A: 70}
	default:
		return transparent
	}
}

func inRoundedRect(x, y, rx, ry, w, h, radius float64) bool {
	right := rx + w
	bottom := ry + h
	if x < rx || x > right || y < ry || y > bottom {
		return false
	}
	cx := clamp(x, rx+radius, right-radius)
	cy := clamp(y, ry+radius, bottom-radius)
	return math.Hypot(x-cx, y-cy) <= radius
}

func onArc(x, y, cx, cy, radius, stroke, startDeg, endDeg float64) bool {
	dx := x - cx
	dy := y - cy
	dist := math.Hypot(dx, dy)
	if math.Abs(dist-radius) > stroke/2 {
		return false
	}
	angle := math.Atan2(dy, dx) * 180 / math.Pi
	if startDeg > endDeg {
		if angle >= startDeg || angle <= endDeg {
			return true
		}
	} else if angle >= startDeg && angle <= endDeg {
		return true
	}
	return onArcCap(x, y, cx, cy, radius, stroke, startDeg) || onArcCap(x, y, cx, cy, radius, stroke, endDeg)
}

func onArcCap(x, y, cx, cy, radius, stroke, deg float64) bool {
	rad := deg * math.Pi / 180
	px := cx + math.Cos(rad)*radius
	py := cy + math.Sin(rad)*radius
	return math.Hypot(x-px, y-py) <= stroke/2
}

func onLine(x, y, x1, y1, x2, y2, stroke float64) bool {
	dx := x2 - x1
	dy := y2 - y1
	len2 := dx*dx + dy*dy
	if len2 == 0 {
		return math.Hypot(x-x1, y-y1) <= stroke/2
	}
	t := clamp(((x-x1)*dx+(y-y1)*dy)/len2, 0, 1)
	px := x1 + t*dx
	py := y1 + t*dy
	return math.Hypot(x-px, y-py) <= stroke/2
}

func clamp(value, min, max float64) float64 {
	if value < min {
		return min
	}
	if value > max {
		return max
	}
	return value
}

func downsample(src *image.NRGBA, size, scale int) image.Image {
	dst := image.NewNRGBA(image.Rect(0, 0, size, size))
	for y := 0; y < size; y++ {
		for x := 0; x < size; x++ {
			var r, g, b, a uint32
			for yy := 0; yy < scale; yy++ {
				for xx := 0; xx < scale; xx++ {
					c := src.NRGBAAt(x*scale+xx, y*scale+yy)
					r += uint32(c.R)
					g += uint32(c.G)
					b += uint32(c.B)
					a += uint32(c.A)
				}
			}
			count := uint32(scale * scale)
			dst.SetNRGBA(x, y, color.NRGBA{
				R: uint8(r / count),
				G: uint8(g / count),
				B: uint8(b / count),
				A: uint8(a / count),
			})
		}
	}
	return dst
}

func pngBytes(img image.Image) ([]byte, error) {
	var buf bytes.Buffer
	if err := png.Encode(&buf, img); err != nil {
		return nil, fmt.Errorf("encode png: %w", err)
	}
	return buf.Bytes(), nil
}

func icoBytes(sizes []int) ([]byte, error) {
	images := make([][]byte, 0, len(sizes))
	for _, size := range sizes {
		data, err := pngBytes(drawIcon(size))
		if err != nil {
			return nil, err
		}
		images = append(images, data)
	}
	var buf bytes.Buffer
	if err := binary.Write(&buf, binary.LittleEndian, uint16(0)); err != nil {
		return nil, err
	}
	if err := binary.Write(&buf, binary.LittleEndian, uint16(1)); err != nil {
		return nil, err
	}
	if err := binary.Write(&buf, binary.LittleEndian, uint16(len(images))); err != nil {
		return nil, err
	}
	offset := 6 + len(images)*16
	for i, data := range images {
		size := sizes[i]
		width := byte(size)
		height := byte(size)
		if size >= 256 {
			width = 0
			height = 0
		}
		buf.WriteByte(width)
		buf.WriteByte(height)
		buf.WriteByte(0)
		buf.WriteByte(0)
		_ = binary.Write(&buf, binary.LittleEndian, uint16(1))
		_ = binary.Write(&buf, binary.LittleEndian, uint16(32))
		_ = binary.Write(&buf, binary.LittleEndian, uint32(len(data)))
		_ = binary.Write(&buf, binary.LittleEndian, uint32(offset))
		offset += len(data)
	}
	for _, data := range images {
		buf.Write(data)
	}
	return buf.Bytes(), nil
}
