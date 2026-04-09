# Section 30 Specification Outline

## Objective

Define the implementation-ready client-tooling contract for ScratchBird so a limited implementation agent can build and maintain clients, tools, installers, and linked APIs without inferring behavior from internal engine code.

## Primary surfaces

1. Connectivity and runtime profiles.
2. Linked and embedded client API boundary.
3. Tool command surfaces and result rendering.
4. Client error, warning, and exit-code behavior.
5. Maintained language-driver baselines.
6. Installer artifact, wizard, and release-stage profiles.
7. Bounded native administrative control surfaces.

## Section structure

1. `CONNECTIVITY_PROFILES_AND_TOOL_RUNTIME.md`
2. `EMBEDDED_AND_LINKED_LIBRARY_API.md`
3. `TOOL_COMMAND_SURFACE_CONTRACTS.md`
4. `CLIENT_ERROR_AND_RESULT_MODEL.md`
5. maintained driver baseline files
6. `INSTALLER_PROFILES_AND_ARTIFACTS.md`
7. `LINUX_SINGLE_FILE_INSTALLER_WIZARD_AND_RELEASE_STAGE_MODEL.md`
8. native control-surface files

## Out of scope

The following are not current implementation authority unless a file in this section states otherwise explicitly:
- universal multi-driver feature parity
- public ABI guarantees for internal listener, parser, or engine IPC contracts
- live migration orchestration parity with commercial cutover systems
- bidirectional replication orchestration through current client tools
- forensic replay automation beyond the documented current surface
