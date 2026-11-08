# GitHub situation and sandbox constraints

## Fork (the open item)
- Goal: user's own GitHub fork of `opentrack/opentrack` holding branch `trackhat-2026.1`.
- The Claude sandbox's GitHub access is a **repo-scoped allowlist** managed by the session proxy:
  - public repos can be cloned (git reads);
  - API archive/zip downloads of repos not attached to the session return 403;
  - repo creation / fork API paths (`POST /repos/.../forks`) are denied ("Write access to this GitHub API path is not permitted");
  - the in-sandbox GitHub token is a placeholder; real access only comes via the `add_repo` tool for specific repos.
- Workable path: the owner forks once in the GitHub UI (or creates an empty repo), then a Claude session attaches it with
  `add_repo` (access = push) and pushes. In Claude Code on the owner's machine none of this applies: just
  `git remote add fork <url> && git push fork trackhat-2026.1`.
- Don't try to bypass the proxy policy (curl with tokens, alternate hosts). Report the denial instead.
- A thin git bundle of the Trackhat commits is provided: `trackhat-2026.1.bundle` (needs upstream's `opentrack-2026.1.0` tag; see handoff `README.md`).

## Sandbox facts (cloud Linux, only relevant if you resume there)
- Network from the shell is an allowlist (package registries + GitHub via proxy); other sites are refused.
- Work lived under `/home/claude/work/`: `opentrack-upstream` (repo), `qt/6.8.3/{gcc_64,mingw_64}`, `cv-win/install`, `oscpack-win`,
  `ort-win`, `ort-dl`, `build` (Linux), `build-win`, `install-win`. None of this persists beyond the session; rebuild with the docs.
- Real camera/Windows testing is impossible there; Wine 9 is available for GUI smoke tests.
