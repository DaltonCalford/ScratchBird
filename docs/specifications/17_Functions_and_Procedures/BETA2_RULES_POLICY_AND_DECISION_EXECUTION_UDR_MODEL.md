# Beta 2 Rules Policy And Decision Execution UDR Model

## Purpose

This document defines the rules and policy UDR family for decision tables,
policy evaluation, explainable rule hits, and deterministic eligibility
workflows.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `Drools` and `Open Policy Agent`.

## Owning package

- `sb_pkg_rules_udr`

## Dependencies

This package depends on:

- `sb_pkg_contract_udr`
- `sb_pkg_text_udr`

## Mandatory surfaces

The package shall provide:

- decision-table definition
- rule-set definition
- policy evaluation
- explainable decision traces
- priority and conflict resolution
- effective-date rule windows

## Required routine families

- `sb_rules.define_table(...)`
- `sb_rules.define_policy(...)`
- `sb_rules.evaluate(...)`
- `sb_rules.explain(...)`
- `sb_rules.conflicts(...)`

## Example contract

```sql
select *
from sb_rules.evaluate(
    policy_id => 'loan_eligibility_v2',
    facts => :application_json
);
```

## Operational rules

1. Rule and policy versions must be immutable by version id.
2. Evaluation traces must capture matched rules, skipped rules, and final
   outcome.
3. Rule evaluation must be deterministic for a fixed policy version and fact
   payload.

## Explicit exclusions

- arbitrary embedded scripting in rule bodies
- remote policy engines as a baseline requirement
