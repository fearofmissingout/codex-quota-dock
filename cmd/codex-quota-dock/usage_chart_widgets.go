//go:build cgo

package main

import (
	"image/color"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

type dailyUsageChartWidget struct {
	widget.BaseWidget
	bars []localUsageDailyBar
}

func newDailyUsageChartWidget() *dailyUsageChartWidget {
	chart := &dailyUsageChartWidget{}
	chart.ExtendBaseWidget(chart)
	return chart
}

func (c *dailyUsageChartWidget) SetBars(bars []localUsageDailyBar) {
	c.bars = append([]localUsageDailyBar(nil), bars...)
	c.Refresh()
}

func (c *dailyUsageChartWidget) CreateRenderer() fyne.WidgetRenderer {
	const count = 7
	columns := make([]*canvas.Rectangle, count)
	values := make([]*canvas.Text, count)
	labels := make([]*canvas.Text, count)
	objects := make([]fyne.CanvasObject, 0, count*3)
	for i := 0; i < count; i++ {
		columns[i] = canvas.NewRectangle(color.NRGBA{R: 82, G: 150, B: 116, A: 110})
		values[i] = canvas.NewText("", theme.ForegroundColor())
		values[i].TextSize = 10
		values[i].Alignment = fyne.TextAlignCenter
		labels[i] = canvas.NewText("", theme.ForegroundColor())
		labels[i].TextSize = 10
		labels[i].Alignment = fyne.TextAlignCenter
		objects = append(objects, columns[i], values[i], labels[i])
	}
	return &dailyUsageChartRenderer{
		widget:  c,
		columns: columns,
		values:  values,
		labels:  labels,
		objects: objects,
	}
}

type dailyUsageChartRenderer struct {
	widget  *dailyUsageChartWidget
	columns []*canvas.Rectangle
	values  []*canvas.Text
	labels  []*canvas.Text
	objects []fyne.CanvasObject
}

func (r *dailyUsageChartRenderer) Layout(size fyne.Size) {
	count := len(r.columns)
	if count == 0 {
		return
	}
	gap := float32(7)
	top := float32(16)
	bottom := float32(30)
	barArea := size.Height - top - bottom
	if barArea < 24 {
		barArea = 24
	}
	slot := (size.Width - gap*float32(count-1)) / float32(count)
	if slot < 12 {
		slot = 12
	}
	barWidth := slot * 0.58
	for i := 0; i < count; i++ {
		x := float32(i)*(slot+gap) + (slot-barWidth)/2
		ratio := float64(0)
		if i < len(r.widget.bars) {
			ratio = r.widget.bars[i].Ratio
		}
		if ratio < 0 {
			ratio = 0
		}
		if ratio > 1 {
			ratio = 1
		}
		height := float32(ratio) * barArea
		if height < 3 {
			height = 3
		}
		y := top + barArea - height
		r.columns[i].Move(fyne.NewPos(x, y))
		r.columns[i].Resize(fyne.NewSize(barWidth, height))
		r.values[i].Move(fyne.NewPos(float32(i)*(slot+gap), 0))
		r.values[i].Resize(fyne.NewSize(slot, 14))
		r.labels[i].Move(fyne.NewPos(float32(i)*(slot+gap), top+barArea+6))
		r.labels[i].Resize(fyne.NewSize(slot, 14))
	}
}

func (r *dailyUsageChartRenderer) MinSize() fyne.Size {
	return fyne.NewSize(330, 132)
}

func (r *dailyUsageChartRenderer) Refresh() {
	for i := range r.columns {
		if i < len(r.widget.bars) {
			bar := r.widget.bars[i]
			r.values[i].Text = bar.Value
			r.labels[i].Text = bar.Label
			if bar.IsToday {
				r.columns[i].FillColor = color.NRGBA{R: 58, G: 143, B: 229, A: 150}
			} else {
				r.columns[i].FillColor = color.NRGBA{R: 64, G: 169, B: 112, A: 135}
			}
		} else {
			r.values[i].Text = "-"
			r.labels[i].Text = "-"
			r.columns[i].FillColor = color.NRGBA{R: 120, G: 130, B: 140, A: 42}
		}
		r.values[i].Color = theme.ForegroundColor()
		r.labels[i].Color = theme.ForegroundColor()
		r.columns[i].Refresh()
		r.values[i].Refresh()
		r.labels[i].Refresh()
	}
}

func (r *dailyUsageChartRenderer) Objects() []fyne.CanvasObject {
	return r.objects
}

func (r *dailyUsageChartRenderer) Destroy() {}

type overallUsageChartWidget struct {
	widget.BaseWidget
	segments []localUsageOverallSegment
}

func newOverallUsageChartWidget() *overallUsageChartWidget {
	chart := &overallUsageChartWidget{}
	chart.ExtendBaseWidget(chart)
	return chart
}

func (c *overallUsageChartWidget) SetSegments(segments []localUsageOverallSegment) {
	c.segments = append([]localUsageOverallSegment(nil), segments...)
	c.Refresh()
}

func (c *overallUsageChartWidget) CreateRenderer() fyne.WidgetRenderer {
	const count = 4
	rects := make([]*canvas.Rectangle, count)
	labels := make([]*canvas.Text, count)
	objects := make([]fyne.CanvasObject, 0, count*2)
	for i := 0; i < count; i++ {
		rects[i] = canvas.NewRectangle(overallSegmentColor(i))
		labels[i] = canvas.NewText("", theme.ForegroundColor())
		labels[i].TextSize = 10
		objects = append(objects, rects[i], labels[i])
	}
	return &overallUsageChartRenderer{
		widget:  c,
		rects:   rects,
		labels:  labels,
		objects: objects,
	}
}

type overallUsageChartRenderer struct {
	widget  *overallUsageChartWidget
	rects   []*canvas.Rectangle
	labels  []*canvas.Text
	objects []fyne.CanvasObject
}

func (r *overallUsageChartRenderer) Layout(size fyne.Size) {
	barY := float32(18)
	barH := float32(26)
	x := float32(0)
	for i, rect := range r.rects {
		width := float32(0)
		if i < len(r.widget.segments) {
			width = size.Width * float32(r.widget.segments[i].Ratio)
		}
		if width < 2 && i < len(r.widget.segments) && r.widget.segments[i].Ratio > 0 {
			width = 2
		}
		rect.Move(fyne.NewPos(x, barY))
		rect.Resize(fyne.NewSize(width, barH))
		x += width
	}
	for i, label := range r.labels {
		row := i / 2
		col := i % 2
		label.Move(fyne.NewPos(float32(col)*size.Width/2, 58+float32(row)*18))
		label.Resize(fyne.NewSize(size.Width/2-6, 16))
	}
}

func (r *overallUsageChartRenderer) MinSize() fyne.Size {
	return fyne.NewSize(330, 104)
}

func (r *overallUsageChartRenderer) Refresh() {
	for i := range r.rects {
		r.rects[i].FillColor = overallSegmentColor(i)
		if i < len(r.widget.segments) {
			segment := r.widget.segments[i]
			r.labels[i].Text = segment.Name + "  " + segment.Value + "  " + segment.Share
		} else {
			r.labels[i].Text = "-"
		}
		r.labels[i].Color = theme.ForegroundColor()
		r.rects[i].Refresh()
		r.labels[i].Refresh()
	}
}

func (r *overallUsageChartRenderer) Objects() []fyne.CanvasObject {
	return r.objects
}

func (r *overallUsageChartRenderer) Destroy() {}

func overallSegmentColor(index int) color.Color {
	colors := []color.NRGBA{
		{R: 65, G: 143, B: 220, A: 150},
		{R: 75, G: 178, B: 121, A: 150},
		{R: 210, G: 143, B: 64, A: 150},
		{R: 177, G: 99, B: 214, A: 150},
	}
	if index < 0 || index >= len(colors) {
		return color.NRGBA{R: 120, G: 130, B: 140, A: 70}
	}
	return colors[index]
}
