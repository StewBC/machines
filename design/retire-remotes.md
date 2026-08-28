# Retire a2m.git and c64m.git

| Field | Value |
|-------|-------|
| **Author** | Grok |
| **Date** | 2026-08-27 |
| **Status** | Landed |
| **Canonical path** | [`design/retire-remotes.md`](retire-remotes.md) |

Stage 11 of [`merge-stage-map.md`](merge-stage-map.md). `machines` is the
only home. Do **not** force-push rewritten history into the old remotes
“to keep them in sync”.

## Frozen SHAs (tagged)

From [`import-revisions.md`](import-revisions.md):

| Remote | GitHub | Freeze tag | Points at |
|--------|--------|------------|-----------|
| a2m | `https://github.com/StewBC/a2m.git` | `frozen-for-machines-d863ad98487639445db16134d260221839cb72e9` | `d863ad98487639445db16134d260221839cb72e9` |
| c64m | `https://github.com/StewBC/c64m.git` | `frozen-for-machines-7f3c1abeb1abc6a5121020cb0650db10ba8e2a0a` | `7f3c1abeb1abc6a5121020cb0650db10ba8e2a0a` |

Those annotated tags were pushed on 2026-08-27.

## What landed in Stage 11

README freeze banners were committed (no history rewrite) and pushed:

| Remote | README commit | Default branch |
|--------|---------------|----------------|
| a2m | `f4fbba6` `docs: freeze — development moved to the machines monorepo` | `master` |
| c64m | `837b827` `docs: freeze — development moved to the machines monorepo` | `main` |

GitHub repos were **archived** (read-only) on 2026-08-27:

```text
https://github.com/StewBC/a2m   isArchived=true
https://github.com/StewBC/c64m  isArchived=true
```

`https://github.com/StewBC/machines` did not exist at Stage 11 land time.
When that GitHub home is published, unarchive briefly if you want to add
the URL to the old READMEs, then re-archive. Do not delete the old repos;
the freeze tags must remain resolvable.

## Owner leftovers (optional)

```bash
# Publish machines if it is still local-only:
#   gh repo create StewBC/machines --source /Users/swessels/Develop/github/personal/machines \
#     --public --push

# If you ever need to unarchive:
#   gh repo unarchive StewBC/a2m --yes
#   gh repo unarchive StewBC/c64m --yes
```

Do not force-push. Do not filter-repo the old remotes to “match” machines.
