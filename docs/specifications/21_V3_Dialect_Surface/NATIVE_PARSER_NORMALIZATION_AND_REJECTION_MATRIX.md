# Native Parser Normalization and Rejection Matrix

## Current code-backed truth
- The parser has real capability-profile options via `ParserOptions`, enabled or disabled feature keys, and deterministic feature gating hooks.
- The lexer uses a real gatekeeper-keyword model that minimizes globally reserved words and pushes many tokens into contextual resolution.
- The conformance corpus includes explicit removed-alias and invalid-surface rejection tests in the public beta gate and native inet suite.

## Capability-state matrix
- `supported`:
  - gatekeeper reserved-keyword set in `lexer_v3.h`
- `supported_parser_surface`:
  - contextual keyword resolution in `parser_v3.h`
- `partial`:
  - feature-key gating closure
  - removed-alias rejection closure
  - naming and quoting cross-dialect closure
  - system-column visibility closure
- `fail_closed`:
  - donor-exact keyword parity or removed-form closure that is not directly re-audited in current tests

## Proven anchors
- `include/scratchbird/parser/parser_v3.h`
- `include/scratchbird/parser/lexer_v3.h`
- `tests/conformance/public_beta/run_required_public_beta_gate.sh`
- `tests/conformance/v3_native_inet`

## Fail-closed boundary
- This document does not claim that every row of the historical rejection matrix has been re-audited.
- Treat deterministic normalization and rejection as `partial`: real framework, partial per-surface proof.
- Removed or legacy alias rejection is only code-backed where parser tests currently exist.
