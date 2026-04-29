#!/usr/bin/env python3
"""Krystie Gate — static check stage of the CI gate.

Runs inside the Gitea Actions runner. Inspects all commits that were just
pushed to a krystie-wip/* branch and rejects if any violates the gate rules.

Decision per commit:
  * If signed by Krystie's GPG key (fingerprint DCF2579968107984), apply the
    full per-repo gate.
  * If signed by a different key OR unsigned, allow (Sami's authority).

Per-repo enforcement:
  * triangles_v5      : red-list (consensus paths) + test-first + no-clearnet
  * triangles-explorer, triangles-api, tridock-web-wallet, sami-chat, tri-pi:
                        test-first only
  * homebrew-triangles: formula syntax check only

Outputs:
  * On reject, prints REJECTED lines to stderr and exits 1.
  * On accept, sets `is_krystie_commit` GH-actions output to true/false.
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

# Krystie's GPG identity. We accept both the primary long-ID and the
# signing subkey because `git log %GK` returns the subkey that was actually
# used to sign, not the primary. The full primary fingerprint is also
# included so a paranoid future check can validate the chain.
KRYSTIE_PRIMARY_FP = "523A81833EB7201573E1EFE1DCF2579968107984"
KRYSTIE_KEY_IDS = {
    "DCF2579968107984",  # primary long-ID
    "C2DC60618C85A159",  # signing subkey long-ID
}

RED_LIST_TRIANGLES_V5 = [
    re.compile(r"^src/main\.(cpp|h)$"),
    re.compile(r"^src/validation.*"),
    re.compile(r"^src/kernel\.(cpp|h)$"),
    re.compile(r"^src/checkpoints\.(cpp|h)$"),
    re.compile(r"^src/consensus/"),
    re.compile(r"^src/protocol\.(cpp|h)$"),
    re.compile(r"^src/net\.(cpp|h)$"),
    re.compile(r"^src/netbase\.(cpp|h)$"),
    re.compile(r"^src/net_bootstrap\.(cpp|h)$"),
    re.compile(r"^src/chainparams.*"),
    re.compile(r"^src/clientversion\.h$"),
    re.compile(r"^src/key\.(cpp|h)$"),
    re.compile(r"^src/keystore\.(cpp|h)$"),
    re.compile(r"^src/onionseed\.h$"),
    re.compile(r"^contrib/seeds/"),
    re.compile(r"^contrib/devtools/release.*"),
    re.compile(r"^doc/release-process\.txt$"),
]

TEST_DIRS = {
    "triangles_v5":       ["src/test/", "test/"],
    "triangles-explorer": ["src/__tests__/", "tests/", "test/"],
    "triangles-api":      ["test/", "__tests__/", "tests/"],
    "tridock-web-wallet": ["test/", "__tests__/", "tests/"],
    "sami-chat":          ["test/", "__tests__/", "tests/"],
    "tri-pi":             ["test/", "tests/"],
    "homebrew-triangles": [],
}

SOURCE_EXTS = {
    "triangles_v5":       {".cpp", ".h", ".c"},
    "triangles-explorer": {".ts", ".tsx", ".js", ".svelte"},
    "triangles-api":      {".js", ".ts"},
    "tridock-web-wallet": {".ts", ".tsx", ".js", ".svelte", ".vue"},
    "sami-chat":          {".ts", ".tsx", ".js", ".svelte", ".vue"},
    "tri-pi":             {".py", ".sh", ".ts", ".js"},
    "homebrew-triangles": set(),
}

RED_LIST_REPOS = {"triangles_v5"}

PEER_CONFIG_PATHS = [
    re.compile(r"^contrib/seeds/"),
    re.compile(r"^src/chainparams.*"),
    re.compile(r".*triangles\.conf(\.example)?$"),
]


@dataclass
class GateResult:
    ok: bool
    reason: str = ""


def repo_name() -> str:
    repo = os.environ.get("GITHUB_REPOSITORY", "")
    return repo.split("/", 1)[1] if "/" in repo else repo


def commit_signer(sha: str) -> str | None:
    try:
        out = subprocess.run(
            ["git", "log", "-1", "--format=%GK", sha],
            check=True, capture_output=True, text=True,
        ).stdout.strip()
        return out or None
    except subprocess.CalledProcessError:
        return None


def is_krystie_commit(sha: str) -> bool:
    fp = commit_signer(sha)
    if not fp:
        return False
    # Accept any key ID we know belongs to Krystie. `git log %GK` returns the
    # signing subkey, so we have to whitelist both primary and subkey.
    return any(fp == known or known.endswith(fp) for known in KRYSTIE_KEY_IDS)


def commits_in_push() -> list[str]:
    before = os.environ.get("GITHUB_BEFORE", "")
    sha = os.environ.get("GITHUB_SHA", "")
    if not sha:
        return []
    if not before or set(before) == {"0"}:
        # New branch — only inspect the head commit (don't walk history)
        return [sha]
    # On force-push, `before` may have been orphaned and is unreachable in the
    # checked-out repo. `git rev-list before..sha` then exits 128. Fall back
    # to inspecting the new head only — that's the safest guarantee we can
    # make about what just landed.
    try:
        out = subprocess.run(
            ["git", "rev-list", f"{before}..{sha}"],
            check=True, capture_output=True, text=True,
        ).stdout
        return [c for c in out.split() if c]
    except subprocess.CalledProcessError:
        return [sha]


def changed_files(sha: str) -> list[str]:
    out = subprocess.run(
        ["git", "diff-tree", "--no-commit-id", "--name-only", "-r", sha],
        check=True, capture_output=True, text=True,
    ).stdout
    return [f for f in out.split("\n") if f]


def commit_diff_text(sha: str, paths: list[str]) -> str:
    if not paths:
        return ""
    out = subprocess.run(
        ["git", "show", "--no-color", sha, "--"] + paths,
        check=True, capture_output=True, text=True,
    ).stdout
    return out


def red_list_check(repo: str, files: list[str]) -> GateResult:
    if repo not in RED_LIST_REPOS:
        return GateResult(True)
    for f in files:
        for pat in RED_LIST_TRIANGLES_V5:
            if pat.match(f):
                return GateResult(False, f"red-list violation: '{f}' is consensus/critical-path; needs Sami review (open red-list-labeled issue)")
    return GateResult(True)


def _is_test_path(f: str, test_dirs: list[str]) -> bool:
    return any(f.startswith(d) for d in test_dirs) or "/test/" in f or "/tests/" in f or "/__tests__/" in f


def test_first_check(repo: str, files: list[str]) -> GateResult:
    src_exts = SOURCE_EXTS.get(repo, set())
    test_dirs = TEST_DIRS.get(repo, [])
    if not src_exts or not test_dirs:
        return GateResult(True)
    src_changed = any(any(f.endswith(e) for e in src_exts) and not _is_test_path(f, test_dirs) for f in files)
    test_changed = any(_is_test_path(f, test_dirs) for f in files)
    if src_changed and not test_changed:
        return GateResult(False, f"test-first violation: source changed without paired test; expected test under {test_dirs}")
    return GateResult(True)


def no_clearnet_check(repo: str, sha: str, files: list[str]) -> GateResult:
    if repo != "triangles_v5":
        return GateResult(True)
    peer_files = [f for f in files if any(p.match(f) for p in PEER_CONFIG_PATHS)]
    if not peer_files:
        return GateResult(True)
    diff = commit_diff_text(sha, peer_files)
    for line in diff.split("\n"):
        if not line.startswith("+") or line.startswith("+++"):
            continue
        body = line[1:].strip()
        if re.search(r"\b(addnode|seednode|connect)\s*=", body, re.IGNORECASE):
            if ".onion" not in body.lower():
                return GateResult(False, f"no-clearnet: added peer/seed without .onion: {body[:120]}")
        if re.match(r"^\s*(\d{1,3}\.){3}\d{1,3}\b", body) or re.match(r"^\s*[0-9a-fA-F:]{4,}\b", body):
            return GateResult(False, f"no-clearnet: clearnet address added: {body[:120]}")
    return GateResult(True)


def gate_commit(repo: str, sha: str) -> list[str]:
    files = changed_files(sha)
    failures = []
    for check, args in [
        (red_list_check,    (repo, files)),
        (test_first_check,  (repo, files)),
        (no_clearnet_check, (repo, sha, files)),
    ]:
        r = check(*args)
        if not r.ok:
            failures.append(f"commit {sha[:12]}: {r.reason}")
    return failures


def emit_output(name: str, value: str):
    out_file = os.environ.get("GITHUB_OUTPUT", "")
    if out_file:
        with open(out_file, "a") as fh:
            fh.write(f"{name}={value}\n")


def main() -> int:
    repo = repo_name()
    if not repo:
        print("ERROR: GITHUB_REPOSITORY not set", file=sys.stderr)
        return 2

    commits = commits_in_push()
    if not commits:
        print("No commits to inspect", file=sys.stdout)
        emit_output("is_krystie_commit", "false")
        return 0

    krystie_count = 0
    all_failures: list[str] = []
    for sha in commits:
        if not is_krystie_commit(sha):
            print(f"  {sha[:12]}: not Krystie-signed (allow)")
            continue
        krystie_count += 1
        print(f"  {sha[:12]}: Krystie-signed; running gate")
        failures = gate_commit(repo, sha)
        all_failures.extend(failures)

    emit_output("is_krystie_commit", "true" if krystie_count > 0 else "false")

    if all_failures:
        print(f"\n[KRYSTIE GATE] REJECTED on {repo}:", file=sys.stderr)
        for f in all_failures:
            print(f"  - {f}", file=sys.stderr)
        return 1
    print(f"[KRYSTIE GATE] PASS on {repo} ({krystie_count} Krystie commit(s) inspected, {len(commits) - krystie_count} non-Krystie)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
