# Implementation Notes - HCN-003

## Method
- Reviewed license inputs and existing compatibility findings.
- Converted legal/compliance requirements into release-packaging controls.
- Captured unresolved obligations as explicit gate blockers rather than implicit risk.

## Important Findings
1. LLVM licensing model supports integration with attribution and exception notices.
2. IDPL obligations require explicit notice and source distribution handling for covered code.
3. ScratchBird `LICENSE` contains unresolved placeholders and cannot be treated as release-final.

## Action Policy
- Keep LLVM integration path.
- Do not reimplement LLVM from scratch.
- Enforce unresolved license metadata as release-blocking until corrected.
