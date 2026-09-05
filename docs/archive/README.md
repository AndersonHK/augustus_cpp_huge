# Archived Notes

This folder holds historical issue snapshots, old sync ledgers, and one-off investigation notes that are still useful as evidence but should not be treated as current runtime contracts.

Prefer the maintained top-level docs and `_codex_*` working-memory files for active architecture, current implementation direction, and handoff guidance. Archived notes may contain stale paths, old hypotheses, or already-superseded workaround plans.

Subfolders make the reason for archival explicit:

- `implemented_plans/` contains plans and migration checklists whose implementation and automated gates are complete. Remaining manual observations belong in current regression notes, not by reopening these plans.
- `historical_audits/` contains point-in-time audits whose findings were applied or superseded. They are evidence, not a current inventory.
- `handoffs/` contains dated session handoffs that have been superseded by the live documentation and current worktree.

Active plans and runtime contracts stay directly under `docs/` (or under `temp/` when they are intentionally short-lived). Moving a document here does not promise that every link inside the historical text remains current.
