# Identity Auth Provider and MFA Security Catalog Model

## Purpose

Define the catalog families that back principal identity resolution, authentication-provider policy chains, and MFA state.

## Catalog Families

The current security catalog model includes first-class records for:

- principal accounts
- authentication providers
- authentication policies
- authentication mappings
- authentication attempt logs
- MFA policies
- MFA enrollments
- MFA recovery codes

## Principal Accounts

Principal accounts bind presented identity and source scope to a concrete account row and an authentication policy.

## Auth Providers and Policies

Authentication providers are stored independently from authentication policies. Policies reference ordered provider chains and method or transport controls rather than duplicating provider configuration inline.

## Auth Mappings

Auth mappings translate external authenticated subjects into database identities. They are separate from provider configuration and separate from account tuples.

## Attempt Log

Authentication attempt evidence is catalog-backed and retains provider attribution and outcome.

## MFA Families

MFA policies, enrollments, and recovery codes are distinct catalog row families because:

- policy is reusable
- enrollment is per account and factor
- recovery is per account and code lifecycle

## Integrity Rules

Security catalog rows are not optional sidecars. Any missing required referenced record is a fail-closed condition for the corresponding security workflow.

## Current Proof and Rebuild Boundary

Current code and tests prove these catalog families are already explicit implementation surfaces. This specification reconstructs the authoritative row-family model so security behavior is anchored in catalog authority instead of scattered runtime assumptions.
