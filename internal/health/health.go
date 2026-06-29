package health

import (
	"fmt"
	"os"
	"strings"

	"github.com/fearofmissingout/codex-quota-dock/internal/auth"
	"github.com/fearofmissingout/codex-quota-dock/internal/profile"
)

type Status string

const (
	StatusOK      Status = "ok"
	StatusWarning Status = "warning"
	StatusError   Status = "error"
)

type Row struct {
	Label  string
	Status Status
	Detail string
}

func Inspect(activeAuthPath string, profiles []profile.Profile, startupEnabled bool, appVersion string) []Row {
	rows := []Row{
		{Label: "Version", Status: StatusOK, Detail: appVersion},
	}
	if _, err := os.Stat(activeAuthPath); err != nil {
		rows = append(rows, Row{Label: "Active auth", Status: StatusError, Detail: err.Error()})
	} else if active, err := auth.Load(activeAuthPath); err != nil {
		rows = append(rows, Row{Label: "Active auth", Status: StatusError, Detail: err.Error()})
	} else {
		rows = append(rows, Row{Label: "Active auth", Status: StatusOK, Detail: maskAccountID(active.Tokens.AccountID)})
	}
	status := StatusOK
	detail := fmt.Sprintf("%d saved profiles", len(profiles))
	if len(profiles) == 0 {
		status = StatusWarning
		detail = "no saved profiles"
	}
	rows = append(rows, Row{Label: "Profiles", Status: status, Detail: detail})
	startup := "disabled"
	if startupEnabled {
		startup = "enabled"
	}
	rows = append(rows, Row{Label: "Startup", Status: StatusOK, Detail: startup})
	return rows
}

func Format(rows []Row) string {
	var out strings.Builder
	for _, row := range rows {
		out.WriteString(row.Label)
		out.WriteString(": ")
		out.WriteString(string(row.Status))
		if strings.TrimSpace(row.Detail) != "" {
			out.WriteString(" - ")
			out.WriteString(row.Detail)
		}
		out.WriteByte('\n')
	}
	return strings.TrimRight(out.String(), "\n")
}

func maskAccountID(accountID string) string {
	if len(accountID) <= 6 {
		return accountID
	}
	return "acc_..." + accountID[len(accountID)-6:]
}
