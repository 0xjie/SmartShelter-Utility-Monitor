# Material Inventory

Use this file as the intake control table. Keep the user informed about what each missing material affects and whether work can continue with limitations.

Status values: `missing`, `provided`, `parsed`, `needs_confirmation`, `not_available`, `deferred`.

## Intake Priority Guide

| Priority | Meaning | How to handle |
| --- | --- | --- |
| required | Needed to make core thesis decisions or avoid false claims | Ask early; block formal drafting if absent and no limitation is confirmed |
| strongly_recommended | Improves standards, outline, evidence, or delivery quality | Ask once, explain impact, allow limited continuation if user defers |
| optional | Useful for polish, verification, or richer delivery | Track when available; do not block unless user explicitly requires it |

## School And Advisor Materials

| Material | Priority | Path / Input | Status | Missing impact | Can continue? | User next step | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Thesis title and type | required |  | missing | Cannot finalize outline, chapter model, or output filenames | no for formal drafting | Provide title and thesis type |  |
| School template | required |  | missing | Cannot claim school-format compliance or final layout correctness | limited | Provide `.docx` / `.pdf` path or confirm no template |  |
| Word-count requirement | required |  | missing | Cannot set reliable chapter budgets | limited | Provide total word count or confirm school has no clear requirement |  |
| Final DOCX requirement | required |  | missing | Cannot lock delivery scope | limited | Confirm whether main DOCX and appendix DOCX are needed |  |
| Task book | strongly_recommended |  | missing | Topic scope, objectives, and cover fields may be incomplete | limited | Provide file path or mark unavailable |  |
| Proposal/opening report | strongly_recommended |  | missing | Background, methods, and planned work may lack official source | limited | Provide file path or mark unavailable |  |
| Advisor requirements | strongly_recommended |  | missing | Chapter emphasis and special constraints may be missed | limited | Paste notes or provide file path |  |
| Advisor Word comments | optional |  | missing | Comment-based revision workflow cannot run | yes | Provide commented `.docx` when revision is needed |  |

## Sample And Style Materials

| Material | Priority | Path / Input | Status | Missing impact | Can continue? | User next step | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Sample paper | strongly_recommended |  | missing | Outline rhythm, word distribution, and style imitation are weaker | yes | Provide `.docx` / `.pdf` sample or confirm none |  |
| Existing draft | optional |  | missing | Cannot reuse prior wording or compare revisions | yes | Provide draft path if available |  |
| Style notes | optional |  | missing | Tone and formatting preferences remain generic | yes | Paste school/advisor style notes if any |  |

## Software Project Evidence (when applicable)

| Material | Priority | Path / Input | Status | Missing impact | Can continue? | User next step | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Source code | required |  | missing | Implementation chapters cannot be evidence-based | no for project-specific body | Provide repository path or confirm non-code thesis |  |
| Database schema | strongly_recommended |  | missing | E-R diagram and data-design sections may be blocked | limited | Provide SQL, migration files, model/entity paths, or mark unavailable |  |
| API docs / routes | strongly_recommended |  | missing | Interface and module descriptions may lack source support | limited | Provide API docs, route files, or controller paths |  |
| Screenshots | strongly_recommended |  | missing | System demonstration and figure registry remain incomplete | limited | Provide screenshots or system access/start command |  |
| Test reports | strongly_recommended |  | missing | Cannot claim tests passed or report test results | limited | Provide reports/logs/screenshots or allow test-plan-only writing |  |

## Research / Survey / Experiment Evidence (when applicable)

| Material | Priority | Path / Input | Status | Missing impact | Can continue? | User next step | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Research object / corpus / case materials | required |  | missing | Research scope and factual claims cannot be grounded | no for research-specific body | Provide data, case documents, corpus path, or confirm software-system thesis |  |
| Survey or interview materials | strongly_recommended |  | missing | Survey analysis, findings, and tables may be blocked | limited | Provide questionnaire, interview notes, consent-safe summaries, or mark unavailable |  |
| Experiment protocol and raw results | strongly_recommended |  | missing | Experiment design and result claims cannot be verified | limited | Provide protocol, raw data, logs, or approve design-only writing |  |
| Analysis scripts / notebooks | optional |  | missing | Computation steps may need manual explanation | yes | Provide scripts or notebooks if analysis reproducibility matters |  |
| Experiment/data files | optional |  | missing | Analysis claims and tables may be limited | yes | Provide data path if the thesis needs experiments |  |

## Literature

| Material | Priority | Path / Input | Status | Missing impact | Can continue? | User next step | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| PDF papers | strongly_recommended | `papers/` | missing | Literature pool and verification checklist cannot be built from local PDFs | limited | Provide PDF folder or approve using existing verified references |  |
| Existing reference list | strongly_recommended |  | missing | Citation formatting and cross-reference work starts from scratch | limited | Provide reference list if available |  |

## Intake Questions To Confirm

1. What is the thesis title and thesis type?
2. Is there a school or college template? If yes, provide the path.
3. Is there a word-count requirement?
4. Is final delivery expected as `.docx`, and is an appendix DOCX required?
5. Are there task-book, proposal, or advisor requirements?
6. Is there a sample paper to imitate?
7. Where is the project source code or research evidence?
8. Are screenshots, database schema, API docs, test reports, or literature PDFs available?
