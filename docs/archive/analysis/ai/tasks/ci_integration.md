### Task: CI integration for docs

Goal: Add CI steps to validate docs and regenerate generated sections.

Steps:
1. Add a script `scripts/generate_docs.sh` that runs: spec extraction, code anchor extraction, API generation, implementation references updates, coverage reports, link check.
2. Wire a GitHub Action to run on PR and push to main. Fail on broken links or missing anchors.
3. Cache ctags and extraction artifacts to speed up CI.

Output:
- CI workflow YAML and generator script.
