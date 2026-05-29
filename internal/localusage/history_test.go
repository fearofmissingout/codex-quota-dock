package localusage

import "testing"

func TestRecordAndLoadSwitchHistory(t *testing.T) {
	root := t.TempDir()

	err := RecordSwitch(root, SwitchAttribution{
		At:        mustTime("2026-05-29T11:00:00Z"),
		ProfileID: "pro",
		AccountID: "acc_pro",
		Alias:     "pro",
	})
	if err != nil {
		t.Fatal(err)
	}
	err = RecordSwitch(root, SwitchAttribution{
		At:        mustTime("2026-05-29T09:00:00Z"),
		ProfileID: "company",
		AccountID: "acc_company",
		Alias:     "company",
	})
	if err != nil {
		t.Fatal(err)
	}

	history, err := LoadSwitchHistory(root)
	if err != nil {
		t.Fatal(err)
	}
	if len(history) != 2 {
		t.Fatalf("history=%+v", history)
	}
	if history[0].ProfileID != "company" || history[1].ProfileID != "pro" {
		t.Fatalf("history not sorted: %+v", history)
	}
}
