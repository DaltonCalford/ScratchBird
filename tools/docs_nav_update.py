#!/usr/bin/env python3
import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple


DOCS_ROOT = Path("/workspace/docs").resolve()


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


def find_first_heading(text: str) -> Optional[str]:
    # Return first markdown heading line content without hashes
    for line in text.splitlines():
        if line.startswith("#"):
            # Strip leading hashes and whitespace
            return re.sub(r"^#+\s*", "", line).strip()
    return None


def derive_title_from_filename(path: Path) -> str:
    name = path.stem.replace("-", " ").replace("_", " ")
    return name[:1].upper() + name[1:]


def posix_relpath(src_dir: Path, target: Path) -> str:
    rel = os.path.relpath(str(target), start=str(src_dir))
    return Path(rel).as_posix()


def parse_front_matter(text: str) -> Tuple[Optional[str], Optional[str]]:
    # Returns (front_matter_block, content_without_front_matter)
    if text.startswith("---\n"):
        end = text.find("\n---\n", 4)
        if end != -1:
            fm = text[: end + 5]
            rest = text[end + 5 :]
            return fm, rest
    return None, text


def extract_spec_refs_from_front_matter(front_matter: str) -> List[str]:
    # crude parser: look for line starting with spec_refs:
    # examples: spec_refs: [REQ-FOO-BAR]
    if not front_matter:
        return []
    m = re.search(r"^spec_refs:\s*\[(.*?)\]\s*$", front_matter, flags=re.MULTILINE)
    if not m:
        return []
    inner = m.group(1).strip()
    if not inner:
        return []
    items = [s.strip() for s in inner.split(",")]
    # remove quotes and empties
    cleaned = []
    for it in items:
        it = it.strip().strip("'\"")
        if it:
            cleaned.append(it)
    return cleaned


def compute_breadcrumbs(path: Path, index_titles: Dict[Path, str]) -> Optional[str]:
    # Build crumbs from nearest ancestors that have index.md
    src_dir = path.parent
    # collect (index_path, title)
    crumbs: List[Tuple[Path, str]] = []
    current = src_dir
    while True:
        idx = current / "index.md"
        if idx.exists() and idx in index_titles:
            # Avoid linking to self when the current file is index.md
            if idx.resolve() != path.resolve():
                crumbs.append((idx, index_titles[idx]))
        if current == DOCS_ROOT:
            break
        if DOCS_ROOT in current.parents:
            current = current.parent
        else:
            break
    # We accumulated from leaf->root, reverse to root->leaf
    crumbs.reverse()
    if not crumbs:
        return None
    pieces = []
    for (idx_path, title) in crumbs:
        rel = posix_relpath(src_dir, idx_path)
        pieces.append(f"[{title}]({rel})")
    return " / ".join(pieces)


def has_breadcrumb_line(existing_top: str) -> bool:
    # heuristic: presence of at least two links separated by ' / '
    lines = existing_top.splitlines()
    for i, line in enumerate(lines[:6]):
        if " / " in line and "](" in line and line.count("[") >= 1:
            return True
    return False


def ensure_breadcrumbs(text: str, breadcrumb_line: Optional[str]) -> str:
    if not breadcrumb_line:
        return text
    fm, rest = parse_front_matter(text)
    if fm:
        top_block = rest[:200]
        if has_breadcrumb_line(top_block):
            return text
        insertion = breadcrumb_line + "\n\n"
        return fm + insertion + rest
    else:
        top_block = text[:200]
        if has_breadcrumb_line(top_block):
            return text
        insertion = breadcrumb_line + "\n\n"
        return insertion + text


def find_section_index(lines: List[str], section_title: str) -> Optional[int]:
    pattern = re.compile(rf"^##\s+{re.escape(section_title)}\s*$", flags=re.IGNORECASE)
    for i, line in enumerate(lines):
        if pattern.match(line.strip()):
            return i
    return None


def extract_first_heading_from_file(path: Path) -> Optional[str]:
    try:
        content = read_text(path)
    except Exception:
        return None
    return find_first_heading(content)


def build_related_links(src: Path, all_md_files: List[Path], index_titles: Dict[Path, str]) -> List[Tuple[str, str]]:
    MAX_LINKS = 7
    MIN_LINKS = 3
    related: List[Tuple[str, str]] = []

    src_dir = src.parent
    src_is_index = src.name == "index.md"

    # 1) Parent index
    if src_dir != DOCS_ROOT:
        parent_index = src_dir.parent / "index.md"
        if parent_index.exists():
            title = index_titles.get(parent_index) or extract_first_heading_from_file(parent_index) or derive_title_from_filename(parent_index)
            rel = posix_relpath(src_dir, parent_index)
            related.append((title, rel))

    # 2) Siblings or children depending on index or not
    candidates: List[Path] = []
    if src_is_index:
        # Children in same directory (non-index)
        for p in sorted(src_dir.glob("*.md")):
            if p.name == "index.md":
                continue
            candidates.append(p)
    else:
        # Sibling pages
        for p in sorted(src_dir.glob("*.md")):
            if p == src:
                continue
            candidates.append(p)

    # Exclusions: templates, ai tasks, hidden underscored folders
    def is_excluded(p: Path) -> bool:
        pp = p.as_posix()
        if "/_templates/" in pp or "/ai/" in pp:
            return True
        return False

    for p in candidates:
        if is_excluded(p):
            continue
        title = extract_first_heading_from_file(p) or derive_title_from_filename(p)
        rel = posix_relpath(src_dir, p)
        related.append((title, rel))
        if len(related) >= MAX_LINKS:
            break

    # 3) If not enough, try directory index files in immediate subdirs (for index pages)
    if len(related) < MIN_LINKS and src_is_index:
        for sub in sorted([d for d in src_dir.iterdir() if d.is_dir()]):
            idx = sub / "index.md"
            if idx.exists():
                title = extract_first_heading_from_file(idx) or derive_title_from_filename(idx)
                rel = posix_relpath(src_dir, idx)
                related.append((title, rel))
                if len(related) >= MAX_LINKS:
                    break

    # 4) If still not enough, try the analysis root index
    if len(related) < MIN_LINKS:
        analysis_index = DOCS_ROOT / "analysis" / "index.md"
        if analysis_index.exists():
            title = extract_first_heading_from_file(analysis_index) or derive_title_from_filename(analysis_index)
            rel = posix_relpath(src_dir, analysis_index)
            # Avoid duplicates
            if rel not in [r for _, r in related]:
                related.append((title, rel))

    # Trim to MAX_LINKS
    return related[:MAX_LINKS]


def ensure_related_section(text: str, src: Path, all_md_files: List[Path], index_titles: Dict[Path, str]) -> str:
    lines = text.splitlines()
    if find_section_index(lines, "Related") is not None:
        return text
    related = build_related_links(src, all_md_files, index_titles)
    if not related:
        return text

    # Insert Related after Spec Trace if exists, else append to end
    spec_idx = find_section_index(lines, "Spec Trace")
    insert_at = len(lines)
    if spec_idx is not None:
        # find end of Spec Trace section (next heading or EOF)
        i = spec_idx + 1
        while i < len(lines) and not re.match(r"^##\s+", lines[i]):
            i += 1
        insert_at = i

    related_block = ["", "## Related"] + [f"- [{title}]({href})" for (title, href) in related] + [""]
    new_lines = lines[:insert_at] + related_block + lines[insert_at:]
    return "\n".join(new_lines)


def ensure_spec_trace_from_front_matter(text: str, src: Path) -> str:
    fm, rest = parse_front_matter(text)
    if not fm:
        return text
    spec_refs = extract_spec_refs_from_front_matter(fm)
    if not spec_refs:
        return text
    # Ensure Spec Trace section exists
    lines = rest.splitlines()
    spec_idx = find_section_index(lines, "Spec Trace")
    if spec_idx is not None:
        return text
    # Build Spec Trace section
    # Compute relative path to requirements.md
    reqs_path = DOCS_ROOT / "analysis" / "traceability" / "spec" / "requirements.md"
    rel_to_reqs = posix_relpath(src.parent, reqs_path)
    bullets = []
    for ref in spec_refs:
        anchor = ref.lower()
        bullets.append(f"- [{ref}]({rel_to_reqs}#{anchor})")
    block = ["", "## Spec Trace"] + bullets + [""]
    # Append to end of content (rest)
    new_rest = rest.rstrip("\n") + "\n" + "\n".join(block)
    return fm + new_rest


def ensure_requirement_id_anchors() -> int:
    """Insert explicit HTML anchors for requirement IDs in requirements.md if missing."""
    reqs = DOCS_ROOT / "analysis" / "traceability" / "spec" / "requirements.md"
    if not reqs.exists():
        return 0
    text = read_text(reqs)
    lines = text.splitlines()
    out: List[str] = []
    changed = False
    req_pattern = re.compile(r"^-\s*(REQ-[A-Z0-9][A-Z0-9\-]*)\s*$")
    anchor_pattern = re.compile(r'^<a\s+id="([^"]+)"\s*></a>\s*$')
    for line in lines:
        m = req_pattern.match(line.strip())
        if m:
            req_id = m.group(1)
            anchor_id = req_id.lower()
            # Only add anchor if previous line isn't already the same anchor
            if not (out and anchor_pattern.match(out[-1]) and out[-1].endswith(f'"{anchor_id}"></a>')):
                out.append(f"<a id=\"{anchor_id}\"></a>")
                changed = True
        out.append(line)
    if changed:
        write_text(reqs, "\n".join(out) + ("\n" if text.endswith("\n") else ""))
        return 1
    return 0


def main() -> int:
    # Ensure explicit anchors on requirements list items
    ensure_requirement_id_anchors()
    # Gather all markdown files
    all_md_files = sorted([p for p in DOCS_ROOT.rglob("*.md")])

    # Compute titles for index.md in all directories
    index_titles: Dict[Path, str] = {}
    for p in all_md_files:
        if p.name == "index.md":
            try:
                content = read_text(p)
            except Exception:
                continue
            title = find_first_heading(content) or derive_title_from_filename(p.parent)
            index_titles[p] = title

    modified: List[Path] = []
    for md in all_md_files:
        # Skip templates and ai tasks
        posix = md.as_posix()
        if "/_templates/" in posix or "/ai/" in posix:
            continue
        original = read_text(md)
        breadcrumb_line = compute_breadcrumbs(md, index_titles)
        updated = ensure_breadcrumbs(original, breadcrumb_line)
        updated = ensure_spec_trace_from_front_matter(updated, md)
        updated = ensure_related_section(updated, md, all_md_files, index_titles)
        if updated != original:
            write_text(md, updated)
            modified.append(md)

    print(f"Updated {len(modified)} files with breadcrumbs/related/spec-trace where applicable.")
    for m in modified:
        print(" -", m.relative_to(DOCS_ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())

