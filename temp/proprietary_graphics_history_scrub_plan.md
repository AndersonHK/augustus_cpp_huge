# Proprietary Augustus graphics history scrub plan

Planning document only. Do not execute this procedure until the maintainer explicitly authorizes the coordinated history rewrite.

## Known incident boundary

- First known import commit: `fb138c6440c81a4aa5ad44cc12601484e2089b0d`.
- Scope the removal to proprietary extracted Augustus graphics blobs and generated extraction metadata. Preserve redistribution-safe authored XML bridges, Vespasian-owned art, and unrelated source changes from the same commits.
- Inventory every local and remote branch, tag, pull-request ref, release archive, GitHub Actions artifact, package, cache, and fork that can retain the affected objects.

## Preflight and recovery

1. Freeze merges and announce the rewrite window to every collaborator.
2. Record all remote refs and commit IDs, export repository settings/branch-protection rules, and create an access-controlled mirror backup plus a ref manifest. The backup must not be published or redistributed.
3. Produce an exact path/blob manifest from the incident commit through every reachable descendant. Review every candidate with the maintainer; do not infer proprietary status from the `Graphics` directory alone.
4. Prepare and test a `git filter-repo` path/blob callback against a disposable mirror. Verify source commits unrelated to the assets remain semantically equivalent.

## Rewrite and verification

1. Temporarily relax only the branch protections required for the coordinated force update.
2. Rewrite all affected branches and tags in the disposable mirror, expire reflogs there, repack, and prove the prohibited blob IDs and paths are unreachable.
3. Run a clean clone build/test and scan the full rewritten object database, not only the working tree.
4. Force-push the reviewed rewritten refs in one maintenance window, restore branch protection immediately, and invalidate/delete affected releases, artifacts, caches, and package versions.
5. Ask GitHub Support to purge cached views and unreachable proprietary objects where platform retention requires provider action.

## Collaborator and downstream coordination

- Require fresh clones after the rewrite. Do not merge, pull, or rebase old clones into rewritten history.
- Close or retarget stale pull requests and delete affected remote feature branches where appropriate.
- Notify fork owners and downstream mirrors with the old/new ref manifest and the prohibited blob IDs so they can rewrite or delete their copies.
- Rotate any automation checkout caches and verify deployment/build hosts no longer contain an old clone.

## Completion evidence

- Full ref and object scan finds no prohibited blob ID, generated `Group_*` tree, extracted PNG/XML payload, extraction stamp, or manifest.
- Fresh anonymous clone, release downloads, archive endpoints, pull-request diffs, and GitHub cached file/blob URLs cannot retrieve the removed content.
- Protected branches/tags point to the reviewed rewritten commits; CI builds and the full startup/save gates pass from a fresh clone.
- Maintainer signs off on the affected-ref manifest, collaborator notification, GitHub Support confirmation, and retained private backup disposition.
