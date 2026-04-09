# PKI Lifecycle Cluster Channels

Status: current_authority

## Current supported scope

Current authority covers configured local and explicitly provisioned channel certificates used by the shipped runtime and management channels.

## Required rules

- channel identities must bind to configured trust roots or accepted peer material
- mutual-authentication channels must fail closed when peer validation fails
- certificate reload is allowed only where the channel lifecycle explicitly supports it
- certificate replacement must not silently disable required trust validation

## Unsupported boundary

Automated distributed issuance, autonomous trust propagation, and cluster-wide certificate orchestration beyond explicitly configured current channels are unsupported and must be rejected rather than approximated.
