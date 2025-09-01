#!/usr/bin/env python3
import subprocess, sys, time, os
from pathlib import Path

ROOT = Path('/workspace')
PROGRESS_DIR = ROOT / 'ProjectPlan' / 'progress'
LOG_TEMPLATE = PROGRESS_DIR / 'PROGRESS_LOG_TEMPLATE.md'


def run(cmd):
    print(f"$ {cmd}")
    return subprocess.run(cmd, shell=True)


def append_failure(phase_id: str, test_name: str, commit: str, notes: str):
    log = PROGRESS_DIR / f"alpha_{phase_id.replace('.', '_')}.log.md"
    if not log.exists() and LOG_TEMPLATE.exists():
        # initialize from template
        with open(LOG_TEMPLATE) as f: tmpl = f.read()
        with open(log, 'w') as f: f.write(tmpl)
    with open(log, 'a') as f:
        f.write("\n\n## Failure/Restart Record (if applicable)\n")
        f.write(f"- **Broken Phase**: {phase_id}\n")
        f.write(f"- **Failing Test**: {test_name}\n")
        f.write(f"- **Commit**: {commit}\n")
        f.write(f"- **Summary**: {notes}\n")
        f.write("- **Action**: Handoff to recovery cycle starting at broken phase, then re-run downstream phases sequentially.\n---\n")


def main():
    if len(sys.argv) < 2:
        print("usage: phase_runner.py <PHASE_ID> [ctest-filter]")
        sys.exit(2)
    phase_id = sys.argv[1]
    test_filter = sys.argv[2] if len(sys.argv) > 2 else ''

    # Build
    run('cd /workspace/build && cmake .. -DCMAKE_BUILD_TYPE=Debug')
    r = run('cd /workspace/build && cmake --build . -j')
    if r.returncode != 0:
        append_failure(phase_id, 'build', '<uncommitted>', 'Build failed')
        sys.exit(r.returncode)

    # Run tests (phase-first, then full)
    if test_filter:
        tr = run(f'cd /workspace/build && ctest -R {test_filter} --output-on-failure')
        if tr.returncode != 0:
            # Determine earlier-phase failure assumption: record and exit
            commit = subprocess.check_output('git rev-parse --short HEAD', shell=True, text=True).strip()
            append_failure(phase_id, test_filter, commit, 'Phase-specific tests failed')
            sys.exit(tr.returncode)
    fr = run('cd /workspace/build && ctest --output-on-failure')
    if fr.returncode != 0:
        commit = subprocess.check_output('git rev-parse --short HEAD', shell=True, text=True).strip()
        append_failure(phase_id, 'full-suite', commit, 'Upstream phase regression detected')
        sys.exit(fr.returncode)

    print('Phase runner: all tests passed')


if __name__ == '__main__':
    main()