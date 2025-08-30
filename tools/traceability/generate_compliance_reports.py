#!/usr/bin/env python3
import json
import os
import re
import sys
from collections import defaultdict, Counter


def ensure_pyyaml():
    try:
        import yaml  # noqa: F401
        return True
    except Exception:
        return False


def load_yaml(path):
    try:
        import yaml
    except Exception as exc:
        raise RuntimeError(
            "PyYAML is required. Install with: pip install pyyaml"
        ) from exc
    with open(path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def read_file(path):
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def write_file(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)


def parse_requirements_by_namespace(requirements_md_path):
    text = read_file(requirements_md_path)
    lines = text.splitlines()
    namespaces = {}
    current_ns = None
    for line in lines:
        if line.startswith("### "):
            heading = line[4:].strip()
            # Skip non-namespace headings
            if heading in ("Requirements index (from ProjectPlan)", "Grouped requirements with details"):
                current_ns = None
                continue
            current_ns = heading
            namespaces.setdefault(current_ns, [])
            continue
        if current_ns and line.strip().startswith("- REQ-"):
            # Format: - REQ-XXX
            parts = line.strip().split()
            if parts:
                req_id = parts[1] if len(parts) > 1 else parts[0].lstrip("- ")
                if req_id.startswith("REQ-"):
                    namespaces[current_ns].append(req_id.rstrip(':'))
    return namespaces


def load_spec_map(spec_map_yaml_path):
    data = load_yaml(spec_map_yaml_path)
    # Normalize: ensure each REQ has docs/code/status keys
    normalized = {}
    for req_id, entry in (data or {}).items():
        docs = entry.get("docs") or []
        code = entry.get("code") or []
        status = entry.get("status") or "Unknown"
        normalized[req_id] = {
            "docs": list(docs),
            "code": list(code),
            "status": status,
        }
    return normalized


def load_code_anchors(anchors_json_path):
    with open(anchors_json_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    anchors = data.get("anchors", [])
    by_path = defaultdict(list)
    for a in anchors:
        path = a.get("path")
        if not path:
            continue
        start = a.get("start")
        end = a.get("end")
        kind = a.get("kind")
        symbol = a.get("symbol")
        if start is None:
            continue
        by_path[path].append({
            "start": int(start),
            "end": int(end) if isinstance(end, int) else None,
            "kind": kind,
            "symbol": symbol,
        })
    # Sort anchors by start line per file for efficient range checks
    for path, lst in by_path.items():
        lst.sort(key=lambda x: x["start"])
    return by_path


def parse_code_mapping_entry(entry):
    # Expected formats:
    #  - "path/to/file.h:10-200"
    #  - "path/to/file.h:10" (single line)
    #  - "path/to/file.h" (no lines)
    if ":" in entry:
        path, line_part = entry.split(":", 1)
        path = path.strip()
        m = re.match(r"^(\d+)(?:-(\d+))?$", line_part.strip())
        if m:
            start = int(m.group(1))
            end = int(m.group(2)) if m.group(2) else int(m.group(1))
            return path, start, end
        # Fallback: no parseable lines
        return path, None, None
    return entry.strip(), None, None


def any_anchor_in_range(anchors_by_path, path, start, end):
    if path not in anchors_by_path:
        return False
    if start is None and end is None:
        # Consider file-level mapping as covered if any anchor exists in file
        return len(anchors_by_path[path]) > 0
    if start is None:
        start = 1
    if end is None:
        end = 10**9
    for a in anchors_by_path[path]:
        a_start = a["start"]
        a_end = a.get("end") or a_start
        if a_start <= end and a_end >= start:
            return True
    return False


def compute_req_coverage(namespaces, spec_map, anchors_by_path):
    req_to_namespace = {}
    for ns, reqs in namespaces.items():
        for r in reqs:
            req_to_namespace[r] = ns

    coverage = {}
    for req_id in req_to_namespace.keys():
        mapping = spec_map.get(req_id, {"docs": [], "code": [], "status": "Unknown"})
        docs = mapping.get("docs") or []
        code = mapping.get("code") or []
        status = mapping.get("status") or "Unknown"
        has_doc = len(docs) > 0
        has_code_anchor = False
        for entry in code:
            path, start, end = parse_code_mapping_entry(entry)
            if any_anchor_in_range(anchors_by_path, path, start, end):
                has_code_anchor = True
                break
        coverage[req_id] = {
            "namespace": req_to_namespace[req_id],
            "docs_present": has_doc,
            "code_anchor_present": has_code_anchor,
            "status": status,
            "docs": docs,
            "code": code,
        }
    return coverage


def namespace_to_subproject(ns):
    # Map namespaces to subprojects per docs structure
    ns_upper = ns.upper()
    if ns_upper.startswith("CORE-HEAP") or ns_upper.startswith("CORE-SPACE"):
        return "storage"
    if ns_upper.startswith("TXN-"):
        return "transactions"
    if ns_upper.startswith("CATALOG-"):
        return "catalog"
    if ns_upper.startswith("INDEX-FAMILIES"):
        return "indexing"
    if ns_upper.startswith("FDW") or ns_upper.startswith("DBLINK"):
        return "fdw"
    if (
        ns_upper.startswith("EXEC-ENGINE")
        or ns_upper.startswith("OPT-STAT")
        or ns_upper.startswith("INTEGRITY")
        or ns_upper.startswith("TRIGGERS")
    ):
        return "query-engine"
    if ns_upper.startswith("PSQL-RUNTIME"):
        return "psql"
    if (
        ns_upper.startswith("SERVER")
        or ns_upper.startswith("PROTOCOL")
        or ns_upper.startswith("AUTH")
        or ns_upper.startswith("TLS")
        or ns_upper.startswith("NETWORK")
        or ns_upper.startswith("CONNECTION")
        or ns_upper.startswith("PROVIDER")
    ):
        return "server"
    return "misc"


def aggregate_by_namespace(coverage):
    by_ns = defaultdict(lambda: {
        "total": 0,
        "docs": 0,
        "code": 0,
        "both": 0,
        "status_counts": Counter(),
        "reqs": [],
    })
    for req_id, info in coverage.items():
        ns = info["namespace"]
        by_ns[ns]["total"] += 1
        by_ns[ns]["docs"] += 1 if info["docs_present"] else 0
        by_ns[ns]["code"] += 1 if info["code_anchor_present"] else 0
        by_ns[ns]["both"] += 1 if (info["docs_present"] and info["code_anchor_present"]) else 0
        by_ns[ns]["status_counts"][info["status"]] += 1
        by_ns[ns]["reqs"].append(req_id)
    return by_ns


def aggregate_by_subproject(coverage):
    by_proj = defaultdict(lambda: {"total": 0, "docs": 0, "code": 0, "both": 0, "status_counts": Counter()})
    for req_id, info in coverage.items():
        ns = info["namespace"]
        sp = namespace_to_subproject(ns)
        by_proj[sp]["total"] += 1
        by_proj[sp]["docs"] += 1 if info["docs_present"] else 0
        by_proj[sp]["code"] += 1 if info["code_anchor_present"] else 0
        by_proj[sp]["both"] += 1 if (info["docs_present"] and info["code_anchor_present"]) else 0
        by_proj[sp]["status_counts"][info["status"]] += 1
    return by_proj


def render_dashboard(by_proj, by_ns, total_reqs, docs_root):
    # docs_root is the path from dashboard to docs root (../../)
    lines = []
    lines.append("### Compliance coverage dashboard")
    lines.append("")
    lines.append(f"Total requirements: {total_reqs}")
    lines.append("")
    lines.append("Namespaces:")
    # Sorted by name for stability
    for ns in sorted(by_ns.keys()):
        ns_data = by_ns[ns]
        total = ns_data["total"]
        both = ns_data["both"]
        docs = ns_data["docs"]
        code = ns_data["code"]
        link = f"{docs_root}traceability/coverage/ns-{ns.lower().replace(' ', '-').replace('/', '-')}.md"
        lines.append(f"- {ns}: {both}/{total} fully covered (docs:{docs} code:{code}) → [{ns} coverage]({link})")
    lines.append("")
    lines.append("Subprojects:")
    for sp in sorted(by_proj.keys()):
        sp_data = by_proj[sp]
        total = sp_data["total"]
        both = sp_data["both"]
        docs = sp_data["docs"]
        code = sp_data["code"]
        lines.append(f"- {sp}: {both}/{total} fully covered (docs:{docs} code:{code})")
    lines.append("")
    lines.append(f"More details in [Traceability coverage index]({docs_root}traceability/coverage/index.md)")
    return "\n".join(lines) + "\n"


def render_namespace_page(ns, ns_data, coverage, docs_root_from_ns):
    lines = []
    lines.append(f"### Coverage — {ns}")
    lines.append("")
    total = ns_data["total"]
    both = ns_data["both"]
    docs = ns_data["docs"]
    code = ns_data["code"]
    lines.append(f"Summary: {both}/{total} fully covered (docs:{docs} code:{code})")
    lines.append("")
    lines.append("Requirements:")
    for req_id in sorted(ns_data["reqs"]):
        info = coverage[req_id]
        status = info["status"]
        doc_tag = "yes" if info["docs_present"] else "no"
        code_tag = "yes" if info["code_anchor_present"] else "no"
        # Links to docs entries if present
        doc_links = []
        for d in info["docs"]:
            # d is relative to docs/analysis root
            doc_links.append(f"[{d}]({docs_root_from_ns}{d})")
        doc_links_str = ", ".join(doc_links) if doc_links else "(none)"
        lines.append(f"- {req_id} — status: {status}; docs: {doc_tag} {doc_links_str}; code: {code_tag}")
    lines.append("")
    lines.append(f"Back to [coverage index]({docs_root_from_ns}traceability/coverage/index.md)")
    return "\n".join(lines) + "\n"


def render_coverage_index(by_ns):
    lines = []
    lines.append("### Traceability coverage index")
    lines.append("")
    lines.append("Per-namespace pages:")
    for ns in sorted(by_ns.keys()):
        link = f"ns-{ns.lower().replace(' ', '-').replace('/', '-')}.md"
        lines.append(f"- [{ns}]({link})")
    lines.append("")
    lines.append("Also see: [orphan-code.md](orphan-code.md)")
    return "\n".join(lines) + "\n"


def compute_orphan_public_symbols(anchors_by_path, mapped_code_ranges):
    # mapped_code_ranges: dict path -> list of (start, end)
    orphan = []
    public_kinds = {"class", "struct", "enum", "function", "typedef", "union"}
    for path, anchors in anchors_by_path.items():
        if not path.startswith("include/scratchbird"):
            continue
        ranges = mapped_code_ranges.get(path, [])
        for a in anchors:
            if a.get("kind") not in public_kinds:
                continue
            a_start = a["start"]
            covered = False
            for (s, e) in ranges:
                s2 = s if s is not None else 1
                e2 = e if e is not None else 10**9
                if s2 <= a_start <= e2:
                    covered = True
                    break
            if not covered:
                orphan.append({
                    "symbol": a.get("symbol"),
                    "path": path,
                    "start": a_start,
                })
    return orphan


def build_mapped_code_ranges(spec_map):
    mapped = defaultdict(list)
    for entry in spec_map.values():
        for code in entry.get("code", []) or []:
            path, start, end = parse_code_mapping_entry(code)
            mapped[path].append((start, end))
    return mapped


def render_orphan_code(orphan):
    lines = []
    lines.append("### Orphan public symbols (no REQ mapping)")
    lines.append("")
    if not orphan:
        lines.append("All public symbols are referenced by at least one REQ mapping.")
    else:
        # Sort for stable output
        orphan_sorted = sorted(orphan, key=lambda x: (x["path"], x["symbol"] or "", x["start"]))
        current_path = None
        for item in orphan_sorted:
            if item["path"] != current_path:
                if current_path is not None:
                    lines.append("")
                current_path = item["path"]
                lines.append(f"- {current_path}")
            lines.append(f"  - {item['symbol']} @ L{item['start']}")
    lines.append("")
    return "\n".join(lines) + "\n"


def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    docs_root = os.path.join(repo_root, "docs", "analysis")

    requirements_md_path = os.path.join(docs_root, "traceability", "spec", "requirements.md")
    spec_map_yaml_path = os.path.join(docs_root, "traceability", "mappings", "spec_map.yaml")
    anchors_json_path = os.path.join(docs_root, "traceability", "mappings", "code_anchors.json")

    dashboard_path = os.path.join(docs_root, "project", "compliance", "coverage-dashboard.md")
    coverage_dir = os.path.join(docs_root, "traceability", "coverage")
    coverage_index_path = os.path.join(coverage_dir, "index.md")
    orphan_code_path = os.path.join(coverage_dir, "orphan-code.md")

    # Load inputs
    namespaces = parse_requirements_by_namespace(requirements_md_path)
    spec_map = load_spec_map(spec_map_yaml_path)
    anchors_by_path = load_code_anchors(anchors_json_path)

    coverage = compute_req_coverage(namespaces, spec_map, anchors_by_path)
    by_ns = aggregate_by_namespace(coverage)
    by_proj = aggregate_by_subproject(coverage)

    # Render dashboard (relative path from dashboard to docs root is ../../)
    dashboard_md = render_dashboard(by_proj, by_ns, total_reqs=len(coverage), docs_root="../../")
    write_file(dashboard_path, dashboard_md)

    # Render per-namespace pages and index
    os.makedirs(coverage_dir, exist_ok=True)
    coverage_index_md = render_coverage_index(by_ns)
    write_file(coverage_index_path, coverage_index_md)
    # docs root from ns page back to docs root: ../../../ (traceability/coverage/* -> docs root)
    docs_root_from_ns = "../../../"
    for ns, ns_data in by_ns.items():
        ns_slug = ns.lower().replace(" ", "-").replace("/", "-")
        ns_page_path = os.path.join(coverage_dir, f"ns-{ns_slug}.md")
        ns_md = render_namespace_page(ns, ns_data, coverage, docs_root_from_ns)
        write_file(ns_page_path, ns_md)

    # Orphan code
    mapped_ranges = build_mapped_code_ranges(spec_map)
    orphan = compute_orphan_public_symbols(anchors_by_path, mapped_ranges)
    orphan_md = render_orphan_code(orphan)
    write_file(orphan_code_path, orphan_md)

    print("Generated:")
    print(" -", dashboard_path)
    print(" -", coverage_index_path)
    print(" -", orphan_code_path)
    for ns in sorted(by_ns.keys()):
        ns_slug = ns.lower().replace(" ", "-").replace("/", "-")
        print(" -", os.path.join(coverage_dir, f"ns-{ns_slug}.md"))


if __name__ == "__main__":
    # Optionally install PyYAML if missing and user requested auto-install via env
    if not ensure_pyyaml() and os.environ.get("AUTO_INSTALL_PYYAML", "1") == "1":
        # Try to install silently
        os.system(f"{sys.executable} -m pip install --quiet pyyaml")
    sys.exit(main())

