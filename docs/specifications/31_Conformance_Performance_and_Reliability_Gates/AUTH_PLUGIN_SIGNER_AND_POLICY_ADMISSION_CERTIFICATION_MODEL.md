Status: current_authority

# Auth Plugin Signer and Policy Admission Certification Model

## Purpose

This file defines the certification lane for signed auth-plugin admission,
especially for enterprise-auth methods.

## Current certified refusal scenarios

The current certification lane already proves:

1. enterprise method plugin with untrusted signer is rejected
2. enterprise method plugin with trusted signer but policy-mismatched signer is rejected

## Current artifacts under test

The current test lane constructs:

1. manifest JSON
2. manifest JWS
3. module path and module hash
4. auth plugin policy JSON
5. truststore reference

## Pass criteria

For the covered scenarios, the certification lane requires:

1. manager initialization succeeds with the supplied truststore and policy files
2. the plugin is not admitted
3. admission issues contain the expected deterministic refusal reason

## Deterministic refusal reasons

The current certification lane relies on at least:

1. `AUTH_PLUGIN_SIGNER_UNTRUSTED`
2. `AUTH_PLUGIN_POLICY_DENIED`

## Enterprise-signer boundary

The certification model makes the following explicit:

1. enterprise method families are gated by enterprise signer policy
2. global trust does not override per-plugin signer policy

## Reconstructed required expansion

The rebuild requires future certification scenarios for:

1. disallowed method ID under otherwise trusted plugin
2. version-band mismatch
3. required plugin missing
4. fail-on-unlisted plugin mode
