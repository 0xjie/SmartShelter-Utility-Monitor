# Thesis Workflow Status

Generated: 2026-05-19

## Current State

```yaml
phase: intake_only
status: in_progress
current_owner: AI + user
next_action:
  - Fill material inventory.
  - Fill `standard-profile.yaml`, `thesis-ai-spec.yaml`, and evidence inputs.
blocked_reason: []
missing_materials: []
can_continue_with_limitations: true
```

## Stage Tracker

| Stage | Status | Output | Notes |
| --- | --- | --- | --- |
| intake_materials | in_progress | material inventory | Upload school template, task book, draft, code, PDFs, screenshots |
| init_workspace | done | `thesis-ai-standard/`, `paper-context/workflow/` | Created by this script |
| resolve_standards | pending | `standard-profile.yaml` | School/advisor rules first |
| analyze_sample_and_template | pending | sample/template analysis | Must precede thesis spec |
| build_evidence | pending | `paper-context/evidence/` | Source code, tests, screenshots, data |
| stop_and_report | pending | blocker report | Global mechanism |
| build_thesis_spec | pending | `thesis-ai-spec.yaml` | Facts only from evidence |
| build_figure_registry | pending | `figure-registry.yaml` | Every item needs source and first mention |
| confirm_outline | pending | confirmed outline | Confirm chapters, word count, style |
| draft_chapters | pending | chapter drafts | Evidence first, prose second |
| produce_assets | pending | figures/screenshots/tables | Source and image paths tracked |
| produce_docx | pending | main DOCX + appendix DOCX | Title-based filenames |
| quality_gates | pending | review report | Standards, evidence, references, DOCX |
| delivery_report | pending | delivery report | Include verification and limitations |

## Latest Decision

- None yet.
