# Section 30 Test Contract

Implementation certification for section 30 requires:

1. Connectivity-profile coverage for maintained attach modes and authentication paths.
2. Driver-baseline coverage for every maintained language lane declared in this section.
3. Tool-command coverage for current administrative, query, and session-control flows.
4. Result and error normalization coverage, including exit codes and transaction-error handling.
5. Installer and packaging coverage for every supported artifact profile.
6. Negative tests proving unsupported or internal-only control surfaces fail closed.
7. Transaction-model tests proving client tools preserve the always-in-transaction MGA model.
8. Wizard-page coverage for category selection, right-pane descriptions,
   dependency auto-selection, and disabled release-stage surfaces.
9. Security-bootstrap coverage for local `SysArch`, named `sysadmin`,
   authentication plugin validation, and fail-closed startup refusal.
10. Emulation-family coverage for default donor-port assignment, bind-address
    conflict refusal, and engine-wide allowed-network enforcement.
11. Analytical/domain UDR wizard coverage for full section `17` package
    inventory visibility, greyed `planned_disabled` rows, dependency
    auto-selection, and refusal when a disabled dependency blocks a package.
12. Interactive, terminal, and unattended response-file parity coverage.

Future-only or unsupported control families are not test obligations unless promoted into current authority.
