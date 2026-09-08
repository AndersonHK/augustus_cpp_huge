"""Check ledger coverage against Git without treating a listed commit as ported."""
import argparse
import collections
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True, encoding="utf-8").strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default="280df20360d65bb13471aca2369a654f43607a30")
    parser.add_argument("--target", default="upstream/master")
    parser.add_argument("--ledger", type=Path, default=ROOT / "docs/augustus_sync_2026_09_05_ledger.md")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    base, target = git("rev-parse", args.base), git("rev-parse", args.target)
    queue = git("rev-list", "--reverse", f"{base}..{target}").splitlines()
    rows = []
    for line in args.ledger.read_text(encoding="utf-8-sig").splitlines():
        if not re.match(r"^\| `[0-9a-f]{9,40}` \|", line):
            continue
        columns = [value.strip() for value in re.split(r"(?<!\\)\|", line)[1:-1]]
        if len(columns) != 5:
            raise ValueError(f"Expected five ledger columns: {line}")
        commit = git("rev-parse", columns[0].strip("`"))
        rows.append(dict(commit=commit, addition=columns[1], action=columns[2], reason=columns[3], status=columns[4]))
    counts = collections.Counter(row["commit"] for row in rows)
    duplicates = sorted(commit for commit, count in counts.items() if count != 1)
    missing, extra = sorted(set(queue) - counts.keys()), sorted(counts.keys() - set(queue))
    report = dict(base=base, target=target, reachable_commits=len(queue), first_parent_commits=len(git("rev-list", "--first-parent", f"{base}..{target}").splitlines()),
                  missing=missing, extra=extra, duplicates=duplicates,
                  status_counts=dict(collections.Counter(row["status"] for row in rows)), commits=rows)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Ledger coverage: {len(rows)} rows / {len(queue)} reachable commits at {target}.")
    print(f"Missing: {len(missing)}; extra: {len(extra)}; duplicate: {len(duplicates)}.")
    print("Coverage is an inventory check, not evidence of semantic parity or completed validation.")
    if missing or extra or duplicates:
        print(json.dumps(dict(missing=missing, extra=extra, duplicates=duplicates), indent=2))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
