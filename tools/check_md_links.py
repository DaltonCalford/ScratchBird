#!/usr/bin/env python3
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Set, Tuple


DOCS_ROOT = Path("/workspace/docs").resolve()


@dataclass
class LinkIssue:
    file: Path
    line_no: int
    link_text: str
    target: str
    problem: str


def is_external(href: str) -> bool:
    href_lower = href.lower()
    return href_lower.startswith((
        "http://",
        "https://",
        "mailto:",
        "tel:",
        "ftp://",
        "ftps://",
        "ssh://",
        "git://",
        "news:",
        "irc://",
        "chrome://",
        "about:",
        "data:",
        "file:",
    ))


def slugify_heading(text: str) -> str:
    # GitHub-like slugify: lower, remove punctuation except spaces and hyphens, spaces->hyphens
    t = text.strip().lower()
    # remove backticks and code formatting
    t = t.replace("`", "")
    # replace & with 'and'
    t = t.replace("&", "and")
    # remove anything not alphanumeric, space, or hyphen
    t = re.sub(r"[^a-z0-9\-\s]", "", t)
    # collapse whitespace to single spaces
    t = re.sub(r"\s+", " ", t)
    # spaces to hyphens
    t = t.replace(" ", "-")
    # collapse multiple hyphens
    t = re.sub(r"-+", "-", t)
    return t.strip("-")


def extract_anchors(md_path: Path) -> Set[str]:
    text = md_path.read_text(encoding="utf-8", errors="ignore")
    anchors: Set[str] = set()
    # HTML anchors
    for m in re.finditer(r"<a\s+id=\"([^\"]+)\"\s*></a>", text):
        anchors.add(m.group(1))
    # Markdown headings
    for line in text.splitlines():
        if line.lstrip().startswith("#"):
            # capture after leading #'s
            m = re.match(r"^#+\s+(.*)$", line.strip())
            if m:
                anchors.add(slugify_heading(m.group(1)))
    return anchors


def iter_markdown_files(root: Path) -> Iterable[Path]:
    for p in sorted(root.rglob("*.md")):
        posix = p.as_posix()
        if "/_templates/" in posix or "/ai/" in posix:
            continue
        yield p


def find_links(md_path: Path) -> List[Tuple[int, str, str]]:
    # Return (line_no, link_text, href), skipping code fences
    text = md_path.read_text(encoding="utf-8", errors="ignore")
    links: List[Tuple[int, str, str]] = []
    in_fence = False
    fence_re = re.compile(r"^```")
    link_re = re.compile(r"(?<!\\)!?\[([^\]]+)\]\(([^\)\s]+)(?:\s+\"[^\"]*\")?\)")
    for i, line in enumerate(text.splitlines(), start=1):
        if fence_re.match(line.strip()):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        for m in link_re.finditer(line):
            link_text = m.group(1)
            href = m.group(2)
            links.append((i, link_text, href))
    return links


def check_links() -> List[LinkIssue]:
    issues: List[LinkIssue] = []
    # cache anchors per file
    anchor_cache: Dict[Path, Set[str]] = {}

    for md in iter_markdown_files(DOCS_ROOT):
        for (line_no, link_text, href) in find_links(md):
            # ignore external
            if is_external(href):
                continue
            # ignore pure anchors to same page
            if href.startswith("#"):
                anchor = href[1:]
                anchors = anchor_cache.setdefault(md, extract_anchors(md))
                if anchor not in anchors:
                    issues.append(LinkIssue(md, line_no, link_text, href, "missing local anchor"))
                continue
            # split fragment
            target_path_str, frag = (href.split("#", 1) + [""])[:2]
            # normalize path
            try:
                target_path = (md.parent / target_path_str).resolve()
            except Exception:
                issues.append(LinkIssue(md, line_no, link_text, href, "invalid path syntax"))
                continue
            # ensure within docs
            if DOCS_ROOT not in target_path.parents and target_path != DOCS_ROOT:
                # allow non-docs references only if file exists
                if not target_path.exists():
                    issues.append(LinkIssue(md, line_no, link_text, href, "target not under docs and missing"))
                continue
            # file existence
            if not target_path.exists():
                issues.append(LinkIssue(md, line_no, link_text, href, "target file not found"))
                continue
            # anchor check
            if frag:
                anchors = anchor_cache.get(target_path)
                if anchors is None:
                    anchors = extract_anchors(target_path)
                    anchor_cache[target_path] = anchors
                if frag not in anchors:
                    issues.append(LinkIssue(md, line_no, link_text, href, "anchor not found in target"))

    return issues


def main(argv: List[str]) -> int:
    issues = check_links()
    if not issues:
        print("No link issues found.")
        return 0
    print(f"Found {len(issues)} link issues:\n")
    for iss in issues:
        rel = iss.file.relative_to(DOCS_ROOT)
        print(f"{rel}:{iss.line_no}: [{iss.link_text}]({iss.target}) -> {iss.problem}")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

