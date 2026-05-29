package quota

type Snapshot struct {
	LimitID              string
	LimitName            string
	PlanType             string
	Primary              *Window
	Secondary            *Window
	RateLimitReachedType string
	Credits              *Credits
}

type Window struct {
	UsedPercent   float64
	WindowMinutes int
	ResetAfterSec int64
	ResetsAt      int64
}

type Credits struct {
	HasCredits bool
	Unlimited  bool
	Balance    string
}

type statusPayload struct {
	PlanType             string                    `json:"plan_type"`
	RateLimit            *rateLimitDetails         `json:"rate_limit"`
	Credits              *creditDetails            `json:"credits"`
	AdditionalRateLimits []additionalRateLimit     `json:"additional_rate_limits"`
	RateLimitReachedType *rateLimitReachedTypeBody `json:"rate_limit_reached_type"`
}

type additionalRateLimit struct {
	LimitName      string            `json:"limit_name"`
	MeteredFeature string            `json:"metered_feature"`
	RateLimit      *rateLimitDetails `json:"rate_limit"`
}

type rateLimitDetails struct {
	Allowed         bool            `json:"allowed"`
	LimitReached    bool            `json:"limit_reached"`
	PrimaryWindow   *windowSnapshot `json:"primary_window"`
	SecondaryWindow *windowSnapshot `json:"secondary_window"`
}

type windowSnapshot struct {
	UsedPercent       float64 `json:"used_percent"`
	LimitWindowSecond int     `json:"limit_window_seconds"`
	ResetAfterSecond  int64   `json:"reset_after_seconds"`
	ResetAt           int64   `json:"reset_at"`
}

type creditDetails struct {
	HasCredits bool   `json:"has_credits"`
	Unlimited  bool   `json:"unlimited"`
	Balance    string `json:"balance"`
}

type rateLimitReachedTypeBody struct {
	Type string `json:"type"`
}
