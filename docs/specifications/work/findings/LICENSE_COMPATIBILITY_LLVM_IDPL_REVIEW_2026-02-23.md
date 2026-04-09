# License Compatibility Review: LLVM and IDPL

Date: 2026-02-23
Scope: Determine whether ScratchBird can integrate LLVM-based native compilation without reimplementing LLVM from scratch.

Note: This is engineering compliance planning, not legal advice.

## Inputs Reviewed
- LLVM license text:
  - `/home/dcalford/CliWork/llvm-project/LICENSE.TXT`
- ScratchBird repository license file:
  - `LICENSE`
- IDPL full text (local copy):
  - `docs/reference/workspace_library/technical_specs/firebird/licenses/IDPL_1_0_FULL_TEXT.txt`

## Key Findings

### 1. LLVM licensing is permissive and integration-friendly
- LLVM is under Apache 2.0 with LLVM exceptions.
- Proof:
  - `/home/dcalford/CliWork/llvm-project/LICENSE.TXT:2`
  - `/home/dcalford/CliWork/llvm-project/LICENSE.TXT:208`
- Engineering implication: embedding/linking LLVM in ScratchBird is generally feasible with proper notice/attribution obligations.

### 2. IDPL obligations apply to covered files/modifications
- IDPL defines larger-work composition and source/notice obligations for covered code modifications.
- Proof:
  - Larger work definition: `docs/reference/workspace_library/technical_specs/firebird/licenses/IDPL_1_0_FULL_TEXT.txt:29`
  - Application/source obligations: `docs/reference/workspace_library/technical_specs/firebird/licenses/IDPL_1_0_FULL_TEXT.txt:149`
  - Source availability obligations: `docs/reference/workspace_library/technical_specs/firebird/licenses/IDPL_1_0_FULL_TEXT.txt:161`
  - Required notices: `docs/reference/workspace_library/technical_specs/firebird/licenses/IDPL_1_0_FULL_TEXT.txt:214`
  - Termination and patent clauses: `docs/reference/workspace_library/technical_specs/firebird/licenses/IDPL_1_0_FULL_TEXT.txt:360`

### 3. ScratchBird LICENSE file appears template-style and incomplete
- Placeholder markers remain for developer and year metadata.
- Proof:
  - `LICENSE:13`
  - `LICENSE:16`
- Engineering implication: release/compliance package is at risk until this is finalized.

## Compatibility Position (Engineering)
- No technical need to reimplement LLVM from scratch based on reviewed license terms.
- Practical path is to integrate LLVM and comply with:
  - LLVM Apache 2.0 + exceptions notices.
  - IDPL obligations for IDPL-covered files and modifications.
- Main risk is procedural compliance, not inherent incompatibility.

## Required Compliance Actions
1. Finalize `ScratchBird/LICENSE` with non-placeholder metadata.
2. Add third-party notices bundle for LLVM in release artifacts.
3. Add compliance checklist for source/notice distribution requirements where IDPL-covered modifications apply.
4. Add CI/release gate that verifies required license files are present in packages/installers.
5. Keep `IDPL_1_0_FULL_TEXT.txt` shipped in installer/legal docs where required by policy.

## Decision Recommendation
- Proceed with LLVM-based implementation.
- Do not pursue from-scratch LLVM reimplementation.
- Treat legal-review signoff as a hard gate before external release distribution.
