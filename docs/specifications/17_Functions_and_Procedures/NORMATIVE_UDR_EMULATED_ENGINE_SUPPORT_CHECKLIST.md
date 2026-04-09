# Normative UDR Emulated Engine Support Checklist

## Capability matrix
- language UDR module registration: `supported`
- language UDR module state transitions: `supported`
- active-module resolution: `supported`
- feature-key enablement checks: `supported`
- compile preflight quota or sandbox fencing: `supported`
- builtin package inventory: `supported`
- operator-managed package lifecycle: `unproven`
- complete engine runtime parity matrix: `unproven`

## Code-backed support today
### Stored routine runtime
- catalog-backed functions and procedures exist
- executor routine invocation exists
- routine bytecode persistence exists

### Language UDR boundary
- module registration exists
- module state transitions exist
- active-module resolution exists
- feature-key enablement checks exist
- compile preflight quota and sandbox fencing exists

### Builtin package inventory
Builtin manifests exist for:
- `scratchbird`
- `firebirdsql`
- `postgresql`
- `mysql`

Builtin package kinds currently evidenced:
- `parser`
- `compiler_udr`
- `emulation_udr`

## Main fail-closed rule
Do not document the emulated-engine UDR layer as a fully closed operator-facing package platform. The audited proof currently supports builtin package manifests plus language-UDR module and compile-boundary logic.
