# Authentication Plugin Architecture and Signed Module ABI

Status: current_authority

## Current extension model

Authentication extensions are admitted only through the configured authentication manager and signed-module policy. Built-in methods remain authoritative unless an explicitly trusted signed extension is configured.

## ABI rules

- extension load happens during bootstrap or supported reload windows, not per session
- unsigned modules are rejected
- modules that do not satisfy the current ABI contract are rejected
- a module may extend authentication method handling but may not bypass canonical principal publication, audit hooks, or listener hardening rules
- a module may not silently weaken channel or proof requirements
