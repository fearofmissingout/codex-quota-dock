//go:build cgo

package main

import (
	"image/color"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

type quotaRuleWidget struct {
	widget.BaseWidget
	text             string
	progress         float64
	hasValue         bool
	warningThreshold int
}

func newQuotaRuleWidget() *quotaRuleWidget {
	rule := &quotaRuleWidget{warningThreshold: 20}
	rule.ExtendBaseWidget(rule)
	return rule
}

func (q *quotaRuleWidget) SetWarningThreshold(threshold int) {
	q.warningThreshold = threshold
	q.Refresh()
}

func (q *quotaRuleWidget) SetText(text string) {
	q.text = text
	q.progress, q.hasValue = quotaRuleProgress(text)
	q.Refresh()
}

func (q *quotaRuleWidget) CreateRenderer() fyne.WidgetRenderer {
	bg := canvas.NewRectangle(color.NRGBA{R: 120, G: 130, B: 140, A: 38})
	fill := canvas.NewRectangle(color.NRGBA{R: 58, G: 172, B: 111, A: 72})
	label := canvas.NewText("", theme.ForegroundColor())
	label.TextSize = 11
	return &quotaRuleRenderer{
		widget: q,
		bg:     bg,
		fill:   fill,
		label:  label,
		objects: []fyne.CanvasObject{
			bg,
			fill,
			label,
		},
	}
}

type quotaRuleRenderer struct {
	widget  *quotaRuleWidget
	bg      *canvas.Rectangle
	fill    *canvas.Rectangle
	label   *canvas.Text
	objects []fyne.CanvasObject
}

func (r *quotaRuleRenderer) Layout(size fyne.Size) {
	r.bg.Resize(size)
	r.fill.Move(fyne.NewPos(0, 0))
	width := float32(0)
	if r.widget.hasValue {
		width = size.Width * float32(r.widget.progress/100)
	}
	r.fill.Resize(fyne.NewSize(width, size.Height))
	r.label.Move(fyne.NewPos(8, 1))
	r.label.Resize(fyne.NewSize(size.Width-12, size.Height))
}

func (r *quotaRuleRenderer) MinSize() fyne.Size {
	return fyne.NewSize(260, 19)
}

func (r *quotaRuleRenderer) Refresh() {
	r.label.Text = r.widget.text
	if !r.widget.hasValue {
		r.fill.FillColor = color.NRGBA{R: 120, G: 130, B: 140, A: 30}
	} else if r.widget.warningThreshold > 0 && r.widget.progress <= float64(r.widget.warningThreshold) {
		r.fill.FillColor = color.NRGBA{R: 226, G: 80, B: 72, A: 95}
	} else {
		r.fill.FillColor = color.NRGBA{R: 58, G: 172, B: 111, A: 82}
	}
	r.bg.Refresh()
	r.fill.Refresh()
	r.label.Color = theme.ForegroundColor()
	r.label.Refresh()
}

func (r *quotaRuleRenderer) Objects() []fyne.CanvasObject {
	return r.objects
}

func (r *quotaRuleRenderer) Destroy() {}
