# Auth Plugin Enterprise Provider Gap Baseline

Date: 2026-03-04  
Ticket: `AUTH-PROD-F01`

## Scope
- Plugin-local baseline audit for enterprise/provider-heavy plugins:
  - `ident`, `radius`, `pam`, `ldap`, `kerberos`
- Marker scan focuses on synthetic directive references and test-only policy toggles.

## Audit Command
```bash
src/security/plugins/auth_plugin_provider_gap_audit.sh
```

## Raw Output
```csv
plugin,file,lines,synthetic_token_refs,policy_test_toggle_refs,total_gap_markers
ident,/home/dcalford/CliWork/ScratchBird/src/security/plugins/ident/ident_plugin.cpp,637,0,0,0
radius,/home/dcalford/CliWork/ScratchBird/src/security/plugins/radius/radius_plugin.cpp,711,8,1,9
pam,/home/dcalford/CliWork/ScratchBird/src/security/plugins/pam/pam_plugin.cpp,602,4,1,5
ldap,/home/dcalford/CliWork/ScratchBird/src/security/plugins/ldap/ldap_plugin.cpp,755,4,1,5
kerberos,/home/dcalford/CliWork/ScratchBird/src/security/plugins/kerberos/kerberos_plugin.cpp,707,0,0,0
```

## Interpretation
1. Highest continuation priority: `radius`, `pam`, and `ldap` (non-zero synthetic/test marker counts).
2. `ident` and `kerberos` have zero matches for these specific markers; they still require provider-driver parity review under F06/F03 acceptance criteria.
3. This scan is a baseline heuristic and not a full behavior proof; continuation tickets F02-F06 add runtime-path verification requirements.

## Continuation Checkpoint (Post F04/F05 Start)
Date: 2026-03-04  
Tickets: `AUTH-PROD-F02`, `AUTH-PROD-F04`, `AUTH-PROD-F05`

### Raw Output
```csv
plugin,file,lines,synthetic_token_refs,policy_test_toggle_refs,total_gap_markers
ident,/home/dcalford/CliWork/ScratchBird/src/security/plugins/ident/ident_plugin.cpp,637,0,0,0
radius,/home/dcalford/CliWork/ScratchBird/src/security/plugins/radius/radius_plugin.cpp,732,8,3,11
pam,/home/dcalford/CliWork/ScratchBird/src/security/plugins/pam/pam_plugin.cpp,623,4,3,7
ldap,/home/dcalford/CliWork/ScratchBird/src/security/plugins/ldap/ldap_plugin.cpp,775,4,3,7
kerberos,/home/dcalford/CliWork/ScratchBird/src/security/plugins/kerberos/kerberos_plugin.cpp,707,0,0,0
```

### Interpretation
1. `radius`, `pam`, and `ldap` retain synthetic marker references by design for test-profile execution paths.
2. Increased policy-toggle references reflect explicit production-vs-test runtime profile gate logic (`runtime_profile=test` + policy toggle required).
3. Remaining F-phase closure requires replacing synthetic-primary behavior with provider-driver-backed nominal execution for `ldap`, `radius`, and `pam`, then completing `kerberos`/`ident` parity and integration-matrix gates.

## Continuation Checkpoint (Post F03/F06 Start)
Date: 2026-03-04  
Tickets: `AUTH-PROD-F03`, `AUTH-PROD-F06`

### Raw Output
```csv
plugin,file,lines,synthetic_token_refs,policy_test_toggle_refs,total_gap_markers
ident,/home/dcalford/CliWork/ScratchBird/src/security/plugins/ident/ident_plugin.cpp,689,0,0,0
radius,/home/dcalford/CliWork/ScratchBird/src/security/plugins/radius/radius_plugin.cpp,732,8,3,11
pam,/home/dcalford/CliWork/ScratchBird/src/security/plugins/pam/pam_plugin.cpp,623,4,3,7
ldap,/home/dcalford/CliWork/ScratchBird/src/security/plugins/ldap/ldap_plugin.cpp,775,4,3,7
kerberos,/home/dcalford/CliWork/ScratchBird/src/security/plugins/kerberos/kerberos_plugin.cpp,760,0,0,0
```

### Interpretation
1. `kerberos` and `ident` remain synthetic-directive free (`0` marker counts), matching hardening expectations.
2. `radius`, `pam`, and `ldap` still contain synthetic marker references tied to explicit test-profile-only paths and policy gating.
3. Remaining gap concentration is unchanged: finish nominal provider-driver replacement in `ldap`/`radius`/`pam`, then execute F07 enterprise integration matrix.

## Continuation Checkpoint (F03/F06 Complete, F07 Started)
Date: 2026-03-04  
Tickets: `AUTH-PROD-F03`, `AUTH-PROD-F06`, `AUTH-PROD-F07`

### Raw Output
```csv
plugin,file,lines,synthetic_token_refs,policy_test_toggle_refs,total_gap_markers
ident,/home/dcalford/CliWork/ScratchBird/src/security/plugins/ident/ident_plugin.cpp,689,0,0,0
radius,/home/dcalford/CliWork/ScratchBird/src/security/plugins/radius/radius_plugin.cpp,732,8,3,11
pam,/home/dcalford/CliWork/ScratchBird/src/security/plugins/pam/pam_plugin.cpp,623,4,3,7
ldap,/home/dcalford/CliWork/ScratchBird/src/security/plugins/ldap/ldap_plugin.cpp,775,4,3,7
kerberos,/home/dcalford/CliWork/ScratchBird/src/security/plugins/kerberos/kerberos_plugin.cpp,760,0,0,0
```

### Interpretation
1. `kerberos` and `ident` are now closed for synthetic-directive production concerns and remain marker-clean (`0` counts).
2. `radius`, `pam`, and `ldap` remain the only plugins with synthetic-path markers and are the remaining provider-driver cutover focus for Phase F.
3. Enterprise matrix execution has started (`AUTH-PROD-F07`) using `auth_plugin_enterprise_matrix_gate.sh`; latest pass confirms enterprise targeted suite `6/6` and full plugin regression `26/26`.

## Continuation Checkpoint (F02/F04/F05 Complete, F07/F08 Complete)
Date: 2026-03-04  
Tickets: `AUTH-PROD-F02`, `AUTH-PROD-F04`, `AUTH-PROD-F05`, `AUTH-PROD-F07`, `AUTH-PROD-F08`

### Raw Output
```csv
plugin,file,lines,synthetic_token_refs,policy_test_toggle_refs,total_gap_markers
ident,/home/dcalford/CliWork/ScratchBird/src/security/plugins/ident/ident_plugin.cpp,689,0,0,0
radius,/home/dcalford/CliWork/ScratchBird/src/security/plugins/radius/radius_plugin.cpp,772,8,3,11
pam,/home/dcalford/CliWork/ScratchBird/src/security/plugins/pam/pam_plugin.cpp,659,4,3,7
ldap,/home/dcalford/CliWork/ScratchBird/src/security/plugins/ldap/ldap_plugin.cpp,815,4,3,7
kerberos,/home/dcalford/CliWork/ScratchBird/src/security/plugins/kerberos/kerberos_plugin.cpp,760,0,0,0
```

### Interpretation
1. Marker counts remain concentrated in `radius`, `pam`, and `ldap` because synthetic directives are retained as explicit test-profile-only branches.
2. Provider-driver cutover is now complete for those plugins via dedicated decision helpers (`evaluateRadiusProviderDecision`, `evaluatePamProviderDecision`, `evaluateLdapProviderDecision`) that drive nominal allow/deny/timeout/policy paths.
3. With enterprise matrix gate revalidated (`6/6` targeted, `26/26` full), this baseline now records post-cutover closure evidence rather than open continuation work.
