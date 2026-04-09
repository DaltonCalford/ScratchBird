# Release Compliance Checklist - HCN-003

Date: 2026-02-24
Status: baseline_ready_with_blockers

## Required Controls
- [x] LLVM license source pinned: `/home/dcalford/CliWork/llvm-project/LICENSE.TXT`
- [x] IDPL full text available locally: `docs/reference/workspace_library/technical_specs/firebird/licenses/IDPL_1_0_FULL_TEXT.txt`
- [x] License notice matrix produced: `LICENSE_NOTICE_MATRIX.csv`
- [x] Third-party notice bundle produced: `THIRD_PARTY_NOTICE_BUNDLE.txt`
- [x] Engineering compatibility review recorded: `LICENSE_COMPATIBILITY_LLVM_IDPL_REVIEW_2026-02-23.md`
- [ ] `ScratchBird/LICENSE` placeholder metadata replaced with final values (release-blocking)
- [ ] Legal/compliance signoff recorded in final release evidence (HCN-062)

## Gate Rule
External release packaging is blocked until all unchecked controls are resolved.
