# Certificate mTLS Plugin

[Prev](./05_peer.md) | [Next](./07_jwt_oidc.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin for mutual TLS client-certificate authentication.

Method ID:

- `scratchbird.auth.certificate_x509`

## Dispatch Path

Direct HBA/auth token:

- `cert` (and `clientcert` alias)

## Step 1 - Policy Configuration

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.certificate_mtls": {
  "required": true,
  "allowed_signers": ["sb-release-kid-2026"],
  "min_version": "1.0.0",
  "max_version": "1.x",
  "allowed_method_ids": [
    "scratchbird.auth.certificate_x509"
  ]
}
```

Set policy-value key:

- `auth.certificate_mtls.required_san_prefix`

Example:

- `auth.certificate_mtls.required_san_prefix=dns:db.`

## Step 2 - HBA Routing

```conf
hostssl all             all             0.0.0.0/0              cert
```

Certificate auth should always run over TLS-enabled transport.

## Step 3 - Certificate Assertion Contract

The plugin expects structured assertion data (as tested):

- `sub=<certificate-subject>`
- `san=<san-entry>`
- `chain=trusted|untrusted`
- `revoked=0|1`

Failures include:

- `AUTH_CERT_CHAIN_UNTRUSTED`
- `AUTH_CERT_REVOKED`
- `AUTH_CERT_SAN_POLICY_MISMATCH`
- `AUTH_CERT_SUBJECT_UNKNOWN`

## Step 4 - External PKI Integration

This plugin relies on your PKI process and certificate lifecycle management.

Minimum operator tasks:

1. Define client certificate profile (subject/SAN rules).
2. Publish trusted CA chain to the ScratchBird TLS layer.
3. Implement revocation strategy (CRL/OCSP or equivalent feed into your cert validation pipeline).
4. Keep SAN naming convention aligned with `required_san_prefix`.

## Verification

```bash
ctest --test-dir build -R sb_auth_plugin_certificate_mtls_selftest --output-on-failure
```

Validate positive and negative cert cases (trusted, revoked, SAN mismatch).

## Common Errors

- `AUTH_CERT_SUBJECT_MALFORMED`: invalid subject format in assertion.
- `AUTH_CERT_RESOLVER_UNAVAILABLE`: user resolver hook missing.
- `AUTH_CERT_SUBJECT_UNKNOWN`: certificate subject not mapped to principal.

## Rollback

1. Move affected HBA route from `cert` to SCRAM if PKI outage occurs.
2. Keep TLS transport on even during auth rollback.
3. Restore `cert` only after CA chain/revocation pipeline health is green.

## Completion Checklist

- [ ] SAN prefix policy key set.
- [ ] CA trust and revocation process validated.
- [ ] Subject-to-principal mapping tested.
- [ ] Certificate negative-path tests passed.
