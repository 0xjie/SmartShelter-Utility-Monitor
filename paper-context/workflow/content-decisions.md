# Content Decisions

This file is useful when content candidates are available, but it must not block the main workflow when they are not.

Use it to record what should be emphasized, summarized, moved to appendix, deferred, or excluded. If the user has not provided enough candidates yet, keep the placeholder rows and continue with intake, standards, evidence building, or a provisional outline.

Decision values: `待确认`, `已同意`, `已拒绝`, `待补证据`, `暂缓`.

Suggested use values: `正文重点`, `正文简写`, `附录`, `暂缓`, `不写`.

## Current Content Decisions

| Content | Evidence | Suggested use | Decision | Reason | Notes |
| --- | --- | --- | --- | --- | --- |
| 论文主题和研究对象 | title/type/spec missing | 暂缓 | 待确认 | 需要先确认题目、论文类型和研究对象 | 不阻断材料收集和标准解析 |
| 核心功能或研究主线 | source/evidence pending | 正文重点 | 待补证据 | 通常支撑第 3-5 章，但必须先有证据 | 有源码或材料后再细化 |
| 数据库设计 / E-R 图 | schema missing | 暂缓 | 待补证据 | 仅系统设计类论文通常需要数据设计证据 | 没有 schema 时不要声称 E-R 图完整；非系统设计类可排除 |
| 测试结果或实验结果 | report/log missing | 不写 | 待确认 | 没有报告时不能声称测试通过或实验有效 | 可改写为测试方案或待验证计划 |
| 图表源文件和截图 | figure/screenshot source missing | 附录 | 待补证据 | 有源文件时可进入图表登记和附件 DOCX | 没有时不阻断文字草案，但限制最终交付 |

## Optional Candidate Backlog

Use this section for user-provided modules, diagrams, datasets, experiments, documents, or special advisor requirements that are not yet ready for the decision table.

| Candidate | Source / path | Why it may matter | Next review point |
| --- | --- | --- | --- |
|  |  |  |  |

## Update Rules

- After evidence extraction, update evidence status and suggested use for each candidate.
- Before confirming the outline, ask the user to review items marked `正文重点`, `暂缓`, or `不写`.
- Do not write formal body text from items marked `已拒绝`, `不写`, or `待补证据`.
- If the user cannot provide candidate details, keep this file light and continue the main workflow with clear limitations.
- When a content decision changes, update `user-dashboard.md`, `thesis-ai-spec.yaml`, or `figure-registry.yaml` only if the change affects progress, thesis facts, or figures.
