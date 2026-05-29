//go:build !cgo

package main

import "errors"

func run() error {
	return errors.New("the Fyne desktop UI requires CGO; install a C compiler and build with CGO_ENABLED=1")
}
