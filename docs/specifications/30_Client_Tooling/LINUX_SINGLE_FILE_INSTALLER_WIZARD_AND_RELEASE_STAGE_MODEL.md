# Linux Single-File Installer Wizard and Release Stage Model

## Purpose

Define the canonical Linux installer artifact, wizard flow, privilege model,
security bootstrap pages, emulation-family selection pages, and release-stage
visibility rules for ScratchBird Beta 1 and later promotion waves.

## Scope

This file owns:

- the primary Linux installer artifact model
- interactive GUI and terminal wizard semantics
- unattended response-file parity
- component-category selection pages
- analytical and domain UDR package selection pages
- `SysArch`, named `sysadmin`, auth-plugin, and future cluster bootstrap pages
- emulation-family selection, default donor ports, and allowed-network policy
- disabled or greyed future surfaces that are intentionally visible before they
  are released

## Hard invariants

1. Beta 1 ships a single-file Linux installer bundle as the canonical installer
   artifact.
2. The installer collects operator choices without root privileges and requests
   privilege escalation only at commit time.
3. The installer must create the system user and group `scratchbird` when they
   do not already exist.
4. Static product artifacts are installed into `/opt/scratchbird`; mutable
   state must not be stored in `/opt`.
5. The installer must not complete without a valid local `SysArch` bootstrap
   path.
6. Beta 1 cluster creation and cluster join surfaces are visible but disabled
   unless the release manifest explicitly promotes them.
7. Emulation-family exposure is release-state-driven. A not-yet-released family
   may be visible as disabled but must not be installable.
8. Engine-wide allowed networks constrain every native or emulated front door.
   A per-family listener may narrow but never widen that boundary.
9. Interactive, terminal, and unattended modes must produce the same resolved
   install plan from the same inputs.
10. Root authentication must happen after the final summary and before any
    filesystem, service, or identity mutation.
11. Every section `17` analytical or domain UDR package admitted by canon must
    appear in the installer release manifest and wizard inventory.
12. A UDR package that is specified but not yet implemented must be visible as
    `planned_disabled` rather than omitted, unless it is explicitly marked
    `hidden_internal` by release policy.

## Canonical artifact model

### Beta 1 primary artifact

The primary installer artifact is:

- `scratchbird-installer-<version>-linux-<arch>.run`

The `.run` bundle must contain:

- product manifest and installer schema version
- payload archives for selected product components
- checksums for every embedded payload
- detached signature or embedded signature block
- license texts and notices
- systemd units and uninstall metadata
- response-file schema and defaults
- release manifest describing available versus planned-disabled surfaces
- UDR package manifest describing installable analytical and domain package
  families, dependencies, and release states

### Explicit non-primary artifacts

- `AppImage` is not the canonical Beta 1 installer form.
- Native package-manager lanes such as `deb` or `rpm` may exist later, but they
  are not the canonical Beta 1 installer authority unless a later file promotes
  them explicitly.

## Canonical install layout

The installer must provision:

- static install root: `/opt/scratchbird`
- versioned product root: `/opt/scratchbird/versions/<version>`
- current symlink or pointer: `/opt/scratchbird/current`
- configuration root: `/etc/scratchbird`
- data root: `/var/lib/scratchbird`
- log root: `/var/log/scratchbird`
- runtime root: `/run/scratchbird`

Directory ownership rules:

- product payload trees under `/opt/scratchbird/versions/<version>` are
  root-owned and not runtime-mutable
- mutable data, logs, runtime sockets, and service-owned generated files are
  owned by `scratchbird:scratchbird`
- configuration files containing secrets are root-owned and readable only by
  the required service account

## Release-state vocabulary

Every page, category item, auth plugin, and emulation family exposed by the
installer must carry one of these states:

- `available`
- `available_default_selected`
- `planned_disabled`
- `hidden_internal`

Rules:

1. `available` items may be selected.
2. `available_default_selected` items start checked but remain user-visible.
3. `planned_disabled` items are shown in a greyed or disabled state with an
   explanatory message and release tag.
4. `hidden_internal` items are not shown to ordinary operators.

The same vocabulary applies to UDR package groups and individual UDR package
rows.

## Wizard shell contract

### Shared wizard layout

For component-category pages, the installer uses:

- a checkmark list on the left
- a description pane on the right
- `Back`, `Next`, and `Cancel` buttons on the bottom edge

When the operator highlights an item, the right pane must show:

- item purpose
- installed artifacts
- required dependencies
- optional dependencies
- estimated disk impact
- release state
- whether elevated privileges are needed
- whether the item is recommended, advanced, or developer-only

Disabled items remain focusable so the right pane can explain why they are not
currently available.

### Front-end parity

The same logical page model must be implemented for:

- graphical wizard mode
- terminal wizard mode
- unattended response-file mode

## Wizard flow

### Page order

1. Welcome
2. License agreement
3. Install mode and profile
4. Deployment mode
5. Paths and service defaults
6. Engine components
7. Emulation support and network front doors
8. Tools
9. Client libraries
10. Documentation and examples
11. Support libraries and integrations
12. Analytical and domain UDR packages
13. Local `SysArch` bootstrap
14. Initial named `sysadmin` bootstrap
15. Authentication plugins
16. Plugin-specific setup wizard pages
17. Cluster registration and cluster-admin bootstrap
18. Final summary and confirmation
19. Privilege escalation
20. Install progress
21. Finish

### Beta 1 release-state rules for pages

- `Standalone` deployment mode is `available_default_selected`.
- `Create new cluster` and `Join existing cluster` are `planned_disabled`
  unless the release manifest promotes them.
- The cluster registration and cluster-admin bootstrap page is visible but
  disabled in Beta 1.
- Emulation families not shipped in the current release are listed as
  `planned_disabled`.
- Section `17` UDR packages not shipped in the current release are listed as
  `planned_disabled`.

## Page details

### Install mode and profile page

The installer must offer:

- `Typical`
- `Developer`
- `Server`
- `Custom`

Profile selection preloads later pages but does not bypass the final summary.

### Deployment mode page

The installer must offer:

- `Standalone`
- `Create new cluster`
- `Join existing cluster`

Beta 1 rule:

- only `Standalone` may be selectable unless the release manifest promotes a
  cluster mode

### Paths and service defaults page

The page must collect:

- data root
- log root
- runtime root
- whether to install systemd units
- whether to enable services on boot
- whether to start services immediately after install
- native listener bind address and port when the native listener is selected
- engine-wide allowed networks as CIDR entries

Default engine-wide allowed networks:

- `127.0.0.1/32`
- `::1/128`

### Engine components page

The page must show installable engine-side categories such as:

- core engine runtime
- embedded libraries
- IPC server
- listener
- manager
- systemd units
- development headers

Dependency rules:

- selecting `listener` requires `IPC server` and `core engine runtime`
- selecting `manager` requires `listener`, `IPC server`, and `core engine
  runtime`
- deselecting a dependency must either deselect dependents or block the change
  with an explanation

### Emulation support and network front doors page

The installer must show one row per emulation family from the release manifest.
Each row must include:

- family name
- release state
- parser package state
- compiler UDR state
- emulation UDR state
- default donor port
- selected bind addresses
- selected allowed networks

Beta 1 minimum available-family rule:

- `FirebirdSQL` uses donor default port `3050`
- `PostgreSQL` uses donor default port `5432`
- `MySQL` uses donor default port `3306`

### Analytical and domain UDR packages page

The installer must expose a dedicated UDR package page after support-library
selection and before security bootstrap.

The page shall use the shared wizard shell contract and shall group package
rows under at least these headings:

- `Core analytical`
- `Scientific/statistical`
- `Symbolic/solver`
- `Columnar/ml`
- `Finance/exactness`
- `Simulation/graph/probability`
- `Science verticals/education`

Each package row must include:

- package name
- package id
- release state
- implementation state
- dependency list
- installed artifacts
- estimated disk impact
- whether example datasets or model packs are included
- whether database-local enablement is required after install

The right pane for the highlighted package must show:

- package purpose
- the reference ecosystem it addresses
- key routine families
- required prerequisite packages
- whether the package is currently installable
- the reason it is disabled when not installable

#### Canonical package inventory for the wizard

The installer must include rows for all current Beta 2 section `17` packages:

Core analytical:

- `sb_pkg_num_array_udr`
- `sb_pkg_expr_udr`
- `sb_pkg_sci_udr`
- `sb_pkg_stats_udr`
- `sb_pkg_symbolic_udr`
- `sb_pkg_opt_udr`
- `sb_pkg_arrow_udr`
- `sb_pkg_ml_udr`
- `sb_pkg_nd_udr`
- `sb_pkg_bayes_udr`

Domain extensions:

- `sb_pkg_fin_udr`
- `sb_pkg_units_udr`
- `sb_pkg_exact_math_udr`
- `sb_pkg_diff_eq_udr`
- `sb_pkg_graph_udr`
- `sb_pkg_prob_udr`
- `sb_pkg_autodiff_udr`
- `sb_pkg_astro_udr`
- `sb_pkg_chem_udr`
- `sb_pkg_edu_math_udr`

#### Dependency rules for the UDR page

The installer must enforce at least these dependencies:

- `sb_pkg_sci_udr` requires `sb_pkg_num_array_udr` and `sb_pkg_expr_udr`
- `sb_pkg_stats_udr` requires `sb_pkg_num_array_udr`
- `sb_pkg_symbolic_udr` requires `sb_pkg_expr_udr`
- `sb_pkg_opt_udr` requires `sb_pkg_num_array_udr`
- `sb_pkg_arrow_udr` has no analytical prerequisite
- `sb_pkg_ml_udr` requires `sb_pkg_num_array_udr`, `sb_pkg_expr_udr`, and
  `sb_pkg_arrow_udr`
- `sb_pkg_nd_udr` requires `sb_pkg_num_array_udr` and `sb_pkg_arrow_udr`
- `sb_pkg_bayes_udr` requires `sb_pkg_prob_udr`, `sb_pkg_stats_udr`, and
  `sb_pkg_autodiff_udr`
- `sb_pkg_fin_udr` requires `sb_pkg_stats_udr`, `sb_pkg_opt_udr`,
  `sb_pkg_prob_udr`, and `sb_pkg_exact_math_udr`
- `sb_pkg_units_udr` has no analytical prerequisite
- `sb_pkg_exact_math_udr` has no analytical prerequisite
- `sb_pkg_diff_eq_udr` requires `sb_pkg_sci_udr`, `sb_pkg_prob_udr`,
  `sb_pkg_units_udr`, and `sb_pkg_exact_math_udr`
- `sb_pkg_graph_udr` requires `sb_pkg_num_array_udr` and `sb_pkg_opt_udr`
- `sb_pkg_prob_udr` requires `sb_pkg_num_array_udr` and `sb_pkg_stats_udr`
- `sb_pkg_autodiff_udr` requires `sb_pkg_num_array_udr`,
  `sb_pkg_expr_udr`, and `sb_pkg_symbolic_udr`
- `sb_pkg_astro_udr` requires `sb_pkg_units_udr` and `sb_pkg_nd_udr`
- `sb_pkg_chem_udr` requires `sb_pkg_graph_udr`
- `sb_pkg_edu_math_udr` requires `sb_pkg_symbolic_udr`,
  `sb_pkg_exact_math_udr`, and `sb_pkg_prob_udr`

Selecting a package must auto-select any available dependencies. A dependency
in `planned_disabled` state must prevent the dependent package from becoming
selectable.

#### Beta 1 visibility rule for UDR packages

All UDR package rows listed above must be visible in Beta 1 installers.

- implemented packages may be `available` or `available_default_selected`
- specified but not yet implemented packages must be `planned_disabled`
- internal-only packages may be `hidden_internal` only when the release
  manifest explicitly marks them so

The installer must not silently omit a section `17` Beta 2 package family from
the UDR page merely because it is not yet implemented.

All other planned families may be shown as `planned_disabled` until promoted by
the release manifest.

Each selectable family must allow the operator to choose:

- install family support
- enable family listener after install
- bind addresses
- port override
- family-specific allowed networks that are a subset of the engine-wide
  allowlist

Validation rules:

1. Two enabled listeners on the same node must not resolve to the same bind
   address and port pair.
2. Family-specific allowed networks must be a subset of the engine-wide
   allowlist.
3. A family may be installed but left disabled.
4. A family may not be enabled unless parser, compiler UDR, and emulation UDR
   artifacts are all present and `READY`.

### Tools page

The page must allow selection of operator tools such as:

- `sb_isql`
- `sb_backup`
- `sb_restore`
- `sb_doctor`
- `sb_admin`
- `sb_security`
- `sb_verify`

### Client libraries page

The page must allow grouped selection of shipped client libraries and SDK
artifacts such as:

- C and C++
- Go
- Pascal
- ODBC
- JDBC
- other shipped lanes in the release manifest

### Documentation and examples page

The page must allow selection of:

- admin and operations guides
- developer guides
- SQL and dialect references
- offline HTML docs
- example database and example data

### Support libraries and integrations page

The page must allow selection of released support packages such as:

- DBeaver integration modules
- Metabase integration modules
- bundled driver registration helpers
- shell completions

### Local `SysArch` bootstrap page

This page must:

- explain the reserved bootstrap meaning of `SysArch`
- collect the initial local `SysArch` authentication path
- default to the admitted local password-backed method in Beta 1
- require password and confirmation when local password bootstrap is selected
- offer a strong-password generator
- record whether local recovery remains enabled

The installer must not expose the secret after commit and must not write
plaintext secrets to logs, manifests, or shell history.

### Initial named `sysadmin` bootstrap page

This page must allow creation of one or more named principals that are members
of the `sysadmin` group.

Each row must collect:

- principal name
- selected authentication provider
- provider-specific secret or identity material

The page may allow explicit deferral, but the summary page must warn when no
named `sysadmin` principal is created.

### Authentication plugins page

The page must list builtin and packaged authentication plugins with:

- release state
- whether the plugin can authenticate `SysArch`
- whether the plugin supports MFA
- whether the plugin supports external group mapping
- whether the plugin is startup-critical

Actions:

- `Configure`
- `Test`
- `Reset`
- `Advanced`

Plugin readiness states:

- `unconfigured`
- `configured`
- `validated`
- `enabled`

Rules:

1. A plugin may not be enabled unless it reaches `validated`.
2. Service autostart must be blocked when a startup-critical plugin is selected
   but not validated.
3. At least one enabled plugin must support the selected `SysArch`
   authentication path.

### Plugin-specific setup wizard pages

Each plugin wizard must collect only the fields required by that plugin, such
as:

- endpoint or host
- port
- TLS or certificate references
- bind DN or service identity
- search base or directory scope
- group-to-role mapping rules
- client certificate paths
- token issuer or audience

Each wizard must end with a validation step and a summary step.

### Cluster registration and cluster-admin bootstrap page

This page remains `planned_disabled` in Beta 1 unless promoted explicitly.
When promoted, it must collect:

- cluster id or cluster locator
- seed node or coordinator endpoints
- join token or equivalent registration credential
- node identity material
- cluster-admin attestation or bootstrap method

## Summary, privilege escalation, and execution

### Final summary page

Before requesting root privileges, the installer must show:

- selected profile and deployment mode
- component selections
- selected emulation families
- bind addresses and ports
- engine-wide and per-family allowed networks
- security bootstrap summary without plaintext secrets
- service start and enablement choices
- target filesystem paths

The operator must choose `Continue` or `Cancel`.

### Privilege escalation page

The installer must request privileges only after `Continue`.

Allowed mechanisms:

- terminal mode: `sudo`
- desktop mode: `pkexec` or equivalent approved elevation path

If elevation is denied or cancelled, the installer must exit without partially
mutating the system.

### Install phases

The privileged install path must execute in this order:

1. verify bundle signature and payload checksums
2. create `scratchbird` group if missing
3. create `scratchbird` system user if missing
4. create required directories
5. install payloads under `/opt/scratchbird/versions/<version>`
6. update `/opt/scratchbird/current`
7. write configuration files
8. write security bootstrap material
9. install auth-plugin configuration
10. register services
11. install selected emulation family packages
12. write install manifest and uninstall metadata
13. enable or start services if selected and validated

## Response-file model

Unattended mode must accept a response file that maps one-to-one to the wizard
state.

Example:

```yaml
installer:
  profile: custom
  deployment_mode: standalone
  paths:
    data_root: /var/lib/scratchbird
    log_root: /var/log/scratchbird
    runtime_root: /run/scratchbird
  service_defaults:
    install_systemd_units: true
    enable_on_boot: true
    start_after_install: true
  network:
    engine_allowed_networks: [127.0.0.1/32, ::1/128]
  emulation:
    firebirdsql:
      install: true
      enable_listener: true
      bind_addresses: [127.0.0.1]
      port: 3050
      allowed_networks: [127.0.0.1/32]
    postgresql:
      install: true
      enable_listener: true
      bind_addresses: [127.0.0.1]
      port: 5432
      allowed_networks: [127.0.0.1/32]
    mysql:
      install: false
  security:
    sysarch:
      provider: local_password
      password_source: file:/run/secrets/sysarch.txt
    sysadmin_principals:
      - name: alice_admin
        provider: local_password
        password_source: file:/run/secrets/alice_admin.txt
```

Secrets must be sourced from files, secret stores, or equivalent secure inputs.
Plaintext secret values in command-line arguments are forbidden.

## Deterministic install errors

- `INSTALL_BUNDLE_SIGNATURE_INVALID`
- `INSTALL_PAYLOAD_CHECKSUM_INVALID`
- `INSTALL_PROFILE_INVALID`
- `INSTALL_PROFILE_CONFLICT`
- `INSTALL_RELEASE_STATE_DISABLED`
- `INSTALL_CLUSTER_MODE_NOT_AVAILABLE`
- `INSTALL_SYSARCH_BOOTSTRAP_REQUIRED`
- `INSTALL_SYSARCH_PROVIDER_INVALID`
- `INSTALL_AUTH_PLUGIN_NOT_VALIDATED`
- `INSTALL_PORT_CONFLICT`
- `INSTALL_ALLOWED_NETWORK_INVALID`
- `INSTALL_ALLOWED_NETWORK_OUTSIDE_ENGINE_SCOPE`
- `INSTALL_EMULATION_PACKAGE_INCOMPLETE`
- `INSTALL_PRIVILEGE_ESCALATION_CANCELLED`

## Evidence artifacts

- `docs/specifications/work/conformance/tooling/INSTALL_WIZARD_PAGE_FLOW_RESULTS.csv`
- `docs/specifications/work/conformance/tooling/INSTALL_RELEASE_STAGE_VISIBILITY_RESULTS.csv`
- `docs/specifications/work/conformance/tooling/INSTALL_EMULATION_PORT_AND_NETWORK_RESULTS.csv`
- `docs/specifications/work/conformance/tooling/INSTALL_SECURITY_BOOTSTRAP_RESULTS.md`
- `docs/specifications/work/conformance/tooling/INSTALL_RESPONSE_FILE_PARITY_RESULTS.csv`
