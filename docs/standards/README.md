# Implementation Standards Directory

**Last Updated**: 2024-12-19

This directory contains **MANDATORY** standards and requirements for all implementation work in ScratchBird.

**These standards are ABSOLUTE requirements** - violations are treated as critically as violating MGA_RULES.md.

---

## Quick Start

### Before Starting ANY Implementation

1. Read `/IMPLEMENTATION_STANDARDS.md` (top-level, MANDATORY)
2. Run foundation audit: `./scripts/verify_foundation.sh <feature>`
3. Read all relevant specifications
4. Read this directory's documents relevant to your work

### Before Marking ANY Task Complete

1. Read `/COMPLETION_VERIFICATION_CHECKLIST.md` (top-level, MANDATORY)
2. Run completion check: `./scripts/verify_completion.sh <feature>`
3. Use template from `EVIDENCE_TEMPLATES.md`
4. Provide ALL evidence to user
5. Get user approval

---

## Directory Contents

### Overview Documents

**`README.md`** (this file)
- Directory overview and quick reference
- Links to all standards documents

### Detailed Standards

**`COMMON_FAILURE_PATTERNS.md`** ⚠️ CRITICAL
- **Purpose**: Catalog of failure patterns identified in audits
- **When to read**: Before implementing AND before marking complete
- **Key content**: 8 critical patterns to avoid with detection methods
- **Audience**: All implementers

**`TEST_REQUIREMENTS.md`** ⚠️ MANDATORY
- **Purpose**: Define required test coverage for all features
- **When to read**: Before writing tests
- **Key content**: 6 test types (happy path, restart, negative, multi-path, concurrency, integration)
- **Audience**: All implementers writing tests

**`EVIDENCE_TEMPLATES.md`** 📋 REQUIRED
- **Purpose**: Templates for completion evidence
- **When to read**: Before marking task complete
- **Key content**: Complete evidence package template, format guidelines
- **Audience**: All implementers providing completion evidence

---

## Usage by Phase

### Phase 1: Planning (Before Implementation)

**Read these documents:**
1. `/IMPLEMENTATION_STANDARDS.md` - Overall standards
2. `COMMON_FAILURE_PATTERNS.md` - What to avoid
3. `TEST_REQUIREMENTS.md` - Plan test coverage

**Run these scripts:**
- `./scripts/verify_foundation.sh <feature>` - Verify foundation exists

**Outputs:**
- Foundation audit results
- Test plan
- Requirements checklist

### Phase 2: Implementation (During Development)

**Reference these documents:**
- `COMMON_FAILURE_PATTERNS.md` - Avoid known pitfalls
- `TEST_REQUIREMENTS.md` - Implement required tests

**Follow these processes:**
- Checkpoint system (from `/IMPLEMENTATION_STANDARDS.md`)
- Get user approval at each checkpoint
- Write tests as you implement (not after)

### Phase 3: Completion (Before Marking Done)

**Use these documents:**
1. `/COMPLETION_VERIFICATION_CHECKLIST.md` - Verify all items
2. `EVIDENCE_TEMPLATES.md` - Format evidence

**Run these scripts:**
- `./scripts/verify_completion.sh <feature>` - Automated checks

**Provide to user:**
- Complete evidence package (using template)
- All test outputs
- Catalog verification
- TODO verification

---

## Document Relationships

```
┌─────────────────────────────────────────┐
│     /CLAUDE.md (Session Start)          │
│  - References all mandatory documents   │
└───────────────┬─────────────────────────┘
                │
                ├─────────────────────────────────────┐
                │                                     │
┌───────────────▼──────────────────┐   ┌─────────────▼─────────────────────┐
│ /IMPLEMENTATION_STANDARDS.md     │   │ /COMPLETION_VERIFICATION_         │
│ (Before ANY implementation)      │   │  CHECKLIST.md                     │
│                                  │   │ (Before marking complete)         │
│ References:                      │   │                                   │
│ - Foundation audit script        │   │ References:                       │
│ - Completion checklist           │   │ - Completion script               │
│ - docs/standards/ directory      │   │ - Evidence templates              │
└───────────┬──────────────────────┘   └──────────────┬────────────────────┘
            │                                         │
            │                                         │
┌───────────▼────────────────────────────────────────▼──────────────────────┐
│                     docs/standards/ (This Directory)                      │
│                                                                            │
│  ┌──────────────────────────┐  ┌──────────────────────────┐              │
│  │ COMMON_FAILURE_PATTERNS  │  │  TEST_REQUIREMENTS       │              │
│  │ - What to avoid          │  │  - Required test types   │              │
│  │ - Detection methods      │  │  - Test examples         │              │
│  └──────────────────────────┘  └──────────────────────────┘              │
│                                                                            │
│  ┌──────────────────────────┐  ┌──────────────────────────┐              │
│  │ EVIDENCE_TEMPLATES       │  │  README (this file)      │              │
│  │ - Completion template    │  │  - Directory overview    │              │
│  │ - Format guidelines      │  │  - Quick reference       │              │
│  └──────────────────────────┘  └──────────────────────────┘              │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## Quick Reference by Role

### For Implementers

**Starting a new feature:**
1. Read `/IMPLEMENTATION_STANDARDS.md`
2. Run `./scripts/verify_foundation.sh <feature>`
3. Read `COMMON_FAILURE_PATTERNS.md`
4. Create test plan using `TEST_REQUIREMENTS.md`

**During implementation:**
- Reference `COMMON_FAILURE_PATTERNS.md` to avoid pitfalls
- Implement tests per `TEST_REQUIREMENTS.md`
- Follow checkpoint system

**Before marking complete:**
1. Read `/COMPLETION_VERIFICATION_CHECKLIST.md`
2. Run `./scripts/verify_completion.sh <feature>`
3. Use template from `EVIDENCE_TEMPLATES.md`
4. Provide all evidence to user
5. Get user approval

### For Auditors/Reviewers

**When reviewing implementation:**
1. Check against `/COMPLETION_VERIFICATION_CHECKLIST.md`
2. Verify evidence matches template in `EVIDENCE_TEMPLATES.md`
3. Check for patterns in `COMMON_FAILURE_PATTERNS.md`
4. Verify test coverage per `TEST_REQUIREMENTS.md`

**Red flags:**
- Missing restart test
- No negative tests
- No catalog persistence
- Incomplete evidence
- TODOs remaining
- No user approval

---

## Enforcement

**These standards are ABSOLUTE requirements.**

**Violations include:**
- Starting work without foundation audit
- Marking complete without all test types
- Marking complete without evidence
- Skipping specification reading
- Bypassing checkpoints without user approval

**Treatment**: Violations are as serious as violating MGA_RULES.md and require work to be redone.

---

## Document Maintenance

### When to Update

**Add new failure patterns** to `COMMON_FAILURE_PATTERNS.md` when:
- Audit identifies new recurring issues
- Implementation reveals new pitfalls
- User reports new classes of problems

**Add new test requirements** to `TEST_REQUIREMENTS.md` when:
- New test types become mandatory
- New testing patterns are established
- New integration points require testing

**Update evidence template** in `EVIDENCE_TEMPLATES.md` when:
- New evidence types are required
- Format changes are needed
- New verification steps are added

### Version History

**2024-12-19**: Initial creation
- Created based on audit findings
- Derived from engine_gap_report.md
- Incorporates remediation plan patterns

---

## Integration with Development Workflow

### Git Workflow Integration

**Pre-commit:**
- Verify no TODOs remain (or documented)
- Ensure tests pass

**Pre-pull-request:**
- Run completion verification script
- Provide complete evidence package
- Get user approval

**Pull request checklist:**
- All tests passing (including restart)
- Evidence provided in PR description
- User approved the work

### Continuous Integration

**CI Pipeline should verify:**
- All tests pass
- No new TODOs introduced (or documented)
- Test coverage meets requirements
- Catalog schema changes are documented

---

## Related Top-Level Documents

- `/CLAUDE.md` - Session start instructions (references these standards)
- `/IMPLEMENTATION_STANDARDS.md` - Top-level standards document
- `/COMPLETION_VERIFICATION_CHECKLIST.md` - Pre-completion checklist
- `/MGA_RULES.md` - MGA transaction rules (equally critical)
- `/PROJECT_CONTEXT.md` - Project overview

---

## Related Scripts

- `/scripts/verify_foundation.sh` - Pre-implementation verification
- `/scripts/verify_completion.sh` - Pre-completion verification

---

## Questions and Clarifications

If anything in these standards is unclear:
1. Ask the user for clarification
2. Do NOT proceed with assumptions
3. Document the clarification in appropriate file
4. Update this README if needed

---

## Feedback

These standards were created based on audit findings from production implementation work.
They will evolve as we identify new patterns and requirements.

**To suggest improvements:**
1. Document the specific issue or gap
2. Propose specific change to specific document
3. Provide rationale based on real examples
4. Get user approval before updating
