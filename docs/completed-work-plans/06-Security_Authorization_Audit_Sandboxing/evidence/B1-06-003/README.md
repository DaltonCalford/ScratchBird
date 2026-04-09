# B1-06-003 Evidence

## Scope

Lane A closes the bounded Beta 1 authentication, authorization, masking,
sandbox, and local secret-management surface for package `06`.

## Code Surface Closed

- `src/core/auth_provider.cpp`
- `src/server/server_session.cpp`
- `src/security/tls_context.cpp`
- `src/security/view_security.cpp`
- `tests/unit/test_enterprise_auth_provider_runtime.cpp`
- `tests/unit/test_tls_context_reload.cpp`
- `tests/unit/test_view_security_contract.cpp`

## Canonical Updates

- section `19` auth-method canon now records concrete non-admitted enterprise
  plugin payload methods as fail-closed Beta 1 surfaces
- section `19` provider-chain canon now records negotiation filtering and
  direct runtime refusal for non-admitted enterprise plugin payload methods
- section `19` sandbox canon now records fail-closed view-security behavior for
  `SECURITY DEFINER` and `WITH CHECK OPTION` paths without an integrated
  backend

## Verification

- `lane_a_build.log`
  - rebuilt `scratchbird_tests` after the lane-A code changes
- `lane_a_focus.log`
  - `75` tests from `14` suites ran in `150328 ms`
  - `61` passed
  - `14` skipped because parts of the parity suite are network-gated
- `lane_a_network.log`
  - reran two `AuthPolicyProtocolParityTest` cases with
    `SCRATCHBIRD_TEST_NETWORK=1`
  - `2` passed
- `lane_a_tls_reload_build.log`
  - rebuilt `scratchbird_tests` after the late TLS reload metadata refresh fix
- `lane_a_tls_reload_focus.log`
  - `3` tests from `3` suites ran in `4439 ms`
  - `3` passed

## Result

`B1-06-003` is closed. The bounded Beta 1 auth surface now keeps non-admitted
enterprise plugin payload methods fail-closed, preserves provider-chain lockout
and MFA behavior for the admitted subset, and replaces permissive isolated
view-security success paths with fail-closed sandbox denial. The late closure
pass also proves that in-place TLS certificate reload refreshes cached
operator-facing certificate metadata while explicit database-key rotation
remains the bounded key-lifecycle surface for this package.
