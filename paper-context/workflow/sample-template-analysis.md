# Sample Template Analysis

Use this file to normalize parser outputs from school templates and sample papers into user-readable workflow decisions.

Parser Boundary: PDF and Word parsers are input providers only. If parser output is partial or weak, mark the affected field as `partial`, `unparsed`, or `needs_confirmation`; do not turn parser guesses into confirmed school rules.

## Source Summary

| Source | Type | Parser / input | Parser status | Limitations |
| --- | --- | --- | --- | --- |
|  | school_template / sample_docx / sample_pdf / manual_note |  | pending |  |

## template_rules

Confirmed rules from school templates, advisor notes, or official requirements. These outrank sample patterns.

| Rule | Source | Status | Notes |
| --- | --- | --- | --- |
|  |  | needs_confirmation |  |

## sample_patterns

Observed patterns from prior sample papers. These are references, not hard rules.

| Pattern | Source | Confidence | Notes |
| --- | --- | --- | --- |
| Outline rhythm |  | low | Pending sample analysis |
| Chapter word distribution |  | low | Pending word-count data |
| Figure/table rhythm |  | low | Pending figure/table extraction |

## recommendations

Recommendations for the current thesis. User confirmation is required before formal outline or body writing.

| Recommendation | Based on | Confidence | Needs user confirmation? | Notes |
| --- | --- | --- | --- | --- |
|  |  | low | yes |  |

## Unparsed Or Needs Confirmation

| Item | Reason | Impact | Next action |
| --- | --- | --- | --- |
|  |  |  |  |

## Outline Suggestion

Do not write this directly into `thesis-ai-spec.yaml` until the user confirms it.

| Chapter | Suggested title | Source | Confidence | Needs confirmation | Notes |
| --- | --- | --- | --- | --- | --- |
| 1 |  | template_rules / sample_patterns / thesis_type / user_requirement / default_rule | low | yes |  |

## Source Notes

- `template_rules`: confirmed school or advisor rule.
- `sample_patterns`: observed in sample paper only.
- `thesis_type`: inferred from confirmed thesis type.
- `user_requirement`: explicitly provided by user.
- `default_rule`: fallback when no stronger source is available.

## Word Budget

Use this file for chapter-level word-count planning. Every budget item must name its source and confidence.

If the total word-count requirement is missing, all budgets are provisional.

| Chapter | Suggested words | Source | Basis | Confidence | Status | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| 1 |  | school_requirement / sample_ratio / user_requirement / default_rule |  | low | provisional |  |

## Budget Rules

- School or advisor word-count requirements outrank sample ratios.
- Sample ratios are references only.
- Do not use this file to claim final compliance until the word-count requirement is confirmed.
- Update `user-dashboard.md` when the budget needs user confirmation.
