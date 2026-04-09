# Beta 2 Protected Query Encryption And Enclave Execution Model

## Purpose

Define client-visible protected-query encryption so ScratchBird can support
commercial-style encrypted parameters and encrypted columns while preserving MGA
truth, planner determinism, and auditable refusal behavior.

## Governing rules

1. Protected-query encryption is distinct from at-rest encryption.
2. Protected values remain encrypted outside admitted client-side or protected
   execution lanes.
3. Parser, planner, and executor must know which operator classes are legal for
   each protected profile.
4. Searchable or deterministic protected modes may never silently degrade into
   plaintext shadow columns.

## Protected profiles

The admitted profile families are:

- `DETERMINISTIC_EQ`
  - equality and hash-style grouping only
- `RANDOMIZED_WRITE_ONLY`
  - insert, update, and disclosure only
- `SEARCH_TOKENIZED`
  - equality or pattern search through cataloged token families
- `ENCLAVE_RICH_COMPARE`
  - range, pattern, in-place re-encryption, and selected computed operations
    through a protected enclave lane

## Schema metadata

The canonical metadata row is `sb_protected_column_policy`:

- `policy_uuid`
- `column_uuid`
- `profile_family`
- `key_policy_uuid`
- `token_family`
- `requires_enclave`
- `allowed_operator_mask`
- `result_reveal_class`
- `rotation_policy`
- `status`

## Planner contract

The planner shall classify protected expressions before plan finalization.

Rules:

1. If the operator is not in `allowed_operator_mask`, planning fails closed.
2. `DETERMINISTIC_EQ` columns may use ordinary exact indexes over protected
   bytes only for equality-compatible comparisons.
3. `SEARCH_TOKENIZED` columns may use only the declared token index family.
4. `ENCLAVE_RICH_COMPARE` operators must be isolated into enclave-capable plan
   fragments with explicit admission.

## Protected parameter flow

1. Client or parser receives protected-column metadata.
2. Parser classifies each bound parameter as plaintext, protected payload, or
   enclave request.
3. Protected payload envelope includes:
   - key policy id
   - profile family
   - ciphertext payload
   - token payloads if applicable
   - client algorithm metadata
4. Parser lowers the request into AST and SBLR with protected-parameter flags.
5. Executor refuses the request if the lowered operator class is incompatible
   with the protected profile.

## Enclave lane

The enclave lane is the only admitted place where richer operations may see
plaintext protected values.

Requirements:

- attested enclave identity
- one declared enclave capability manifest
- sealed key handoff into the enclave lane only
- no general-purpose parser or executor access to enclave plaintext buffers
- enclave outputs are limited to approved result classes

## Search-token model

`SEARCH_TOKENIZED` columns shall define:

- token family id
- token granularity
- false-positive posture if approximate matching is used
- token index family
- re-tokenization trigger on key rotation or policy change

The catalog must distinguish searchable tokens from primary ciphertext.

## Rotation and re-encryption

1. Rotation creates a next key policy generation.
2. New writes use the next generation immediately after publication.
3. Existing rows are re-encrypted through one of:
   - client-driven rewrite
   - enclave in-place rewrite
   - protected background rewrite with admitted plaintext exposure only inside
     the enclave lane
4. Old generations retire only after all compatible readers and indexes are
   updated.

## Failure rules

- unsupported operator on protected column:
  `PROTECTED_QUERY_OPERATOR_REFUSED`
- requested rich comparison without enclave:
  `PROTECTED_QUERY_ENCLAVE_REQUIRED`
- failed attestation:
  `PROTECTED_QUERY_ENCLAVE_ATTESTATION_FAILED`
- missing token family metadata:
  `PROTECTED_QUERY_TOKEN_POLICY_INVALID`
- result disclosure not allowed for caller:
  `PROTECTED_QUERY_RESULT_DISCLOSURE_REFUSED`

## Metrics and observability

Expose:

- protected-query requests by profile family
- enclave admissions and refusals
- tokenized search request counts
- protected rotation backlog
- protected operator refusal counts

## Implementation closure requirements

Beta 2 implementation is not complete until all of the following exist:

- parser-visible protected parameter envelopes for text and binary bindings
- catalog lookup cache for `sb_protected_column_policy`
- executor operator-family refusal table
- enclave attestation cache and expiry handling
- token-index maintenance rules for insert, update, delete, and rotation
- result-disclosure post-filter that prevents accidental plaintext return

## Example contracts

```sql
create protected column policy customer_ssn_policy
profile deterministic_eq
on customer.ssn;
select * from customer where ssn = protected_param(:ciphertext_payload);
```

## Sample executor selection logic

```cpp
if (policy.profile_family == DETERMINISTIC_EQ && !expr.is_equality_family()) {
    return refuse(PROTECTED_QUERY_OPERATOR_REFUSED);
}
if (policy.requires_enclave && !session.enclave_ready()) {
    return refuse(PROTECTED_QUERY_ENCLAVE_REQUIRED);
}
return build_protected_plan(expr, policy);
```

## Cross-section requirements

- section 19 owns protected key-policy and enclave admission rules
- section 21 and section 22 own AST and SBLR protected-parameter payloads
- section 33 owns protected memory and enclave scratch accounting
- section 36 owns optimizer trust rules for protected operators and indexes
