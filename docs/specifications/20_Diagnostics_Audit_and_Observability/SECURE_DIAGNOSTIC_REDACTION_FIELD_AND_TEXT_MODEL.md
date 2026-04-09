Status: current_authority

# Secure Diagnostic Redaction Field and Text Model

## Purpose

This file defines the exact diagnostic redaction model used for support bundles,
alert payloads, forensic text, and other operator-visible diagnostics.

## Governing rule

Diagnostics are allowed to expose evidence, but not secrets.

Redaction applies both:

1. by field name
2. by text-pattern recognition

## Current sensitive-field detection

The current sensitive-field detector treats a field name as sensitive when the
normalized key contains tokens including:

- password
- passwd
- pwd
- secret
- token
- apikey
- api_key
- api-key
- credential
- credentials
- authorization
- auth_header
- private_key
- client_secret
- shared_secret
- key_material

## Field-redaction rule

When the field name itself is sensitive, the field value is replaced with:

- `<redacted>`

This is a hard replacement rule, not partial masking.

## Text-redaction rule

When free-form text is sanitized, the current code-backed model redacts:

1. URI userinfo segments
2. bearer or basic authorization payloads
3. sensitive query parameters
4. key-value assignment forms
5. JSON key assignments for sensitive keys
6. SQL-style secret-bearing phrases
7. authority parts of URIs are normalized to endpoint placeholders

## Output markers

The current output markers include:

- `<redacted>`
- `<endpoint>`

These markers are part of the observable contract and are already relied upon by
tests.

## Usage rule

Redaction shall be applied before any support bundle, alert payload summary,
forensic details field, or shadow manifest text is emitted to operator-facing
artifacts.

## Fail-closed rules

The implementation shall not:

1. emit raw passwords after redaction pass
2. emit raw tokens after redaction pass
3. emit embedded URI credentials after redaction pass
4. claim a field is sanitized when the output still contains the raw secret

## Reconstructed required expansion

The rebuild requires future inspection and test lanes for:

1. redaction reason tagging
2. per-field redaction counts by secret class
3. structured redaction audit rows for support-bundle generation
