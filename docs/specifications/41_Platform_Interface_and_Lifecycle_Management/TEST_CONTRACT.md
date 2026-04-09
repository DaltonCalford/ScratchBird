# Section 41 Test Contract

Section `41` is implementation-ready only if maintained evidence covers the
current platform and lifecycle behaviors it claims.

## Required certification lanes

- platform abstraction
  - supported OS and runtime assumptions are enforced deterministically
  - unsupported platform paths fail closed
- filesystem, process, and network assumptions
  - startup and runtime refusal paths are deterministic when required platform
    assumptions are violated
- packaging and deployment
  - documented packaging, launch, and deployment workflows match the shipped
    runtime surfaces
- upgrade and downgrade compatibility
  - compatibility-floor, version, and lifecycle rules are enforced as the
    section describes
- portability exclusions
  - unsupported portability claims are not surfaced as supported runtime
    behavior

## Negative requirements

- no test may assume universal OS parity if section `41` explicitly narrows the
  supported platform set
- no test may infer seamless downgrade or cross-build portability beyond the
  stated compatibility boundary
