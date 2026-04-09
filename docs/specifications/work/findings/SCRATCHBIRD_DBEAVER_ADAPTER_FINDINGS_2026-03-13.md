# ScratchBird DBeaver Adapter Findings
Date: 2026-03-13
Status: Working draft (non-authoritative findings report)

## Purpose

Document what DBeaver actually requires for first-class ScratchBird support,
using the local `ScratchBird-driver` integration module and the local `dbeaver`
source clone as proof anchors.

## Scope

1. `ScratchBird-driver` DBeaver integration assets.
2. DBeaver plugin/extension contracts for JDBC ecosystem adapters.
3. Recursive schema-directory behavior for dotted ScratchBird schemas.
4. Packaging and source-checkout onboarding requirements.

## Method

1. Inspect the existing ScratchBird DBeaver integration module under
   `tracks/alpha/integrations/scratchbird-dbeaver-driver`.
2. Inspect DBeaver extension schemas and registry code in the local
   `~/CliWork/dbeaver` checkout.
3. Compare the integration module against the local DBeaver working tree.
4. Cross-check the JDBC metadata surface used by the adapter.

## Executive Summary

1. DBeaver support for ScratchBird is not a JDBC-JAR-only problem; it requires
   an Eclipse plugin contributing `org.jkiss.dbeaver.dataSourceProvider` and
   `org.jkiss.dbeaver.generic.meta` extensions.
2. Recursive schema directories are handled in DBeaver's navigator metadata
   tree, not by the `org.jkiss.dbeaver.fileSystem` extension point.
3. The local `ScratchBird-driver` repo already contains the correct source home
   for this work at
   `tracks/alpha/integrations/scratchbird-dbeaver-driver`.
4. The local `dbeaver` checkout already contains a mirrored ScratchBird plugin
   and test plugin, but it is a dirty worktree and currently has a duplicate
   `org.jkiss.dbeaver.ext.scratchbird` entry in
   `features/org.jkiss.dbeaver.db.feature/feature.xml`.
5. The current adapter is only a foundation layer. A complete DBeaver-native
   implementation also needs database-specific SQL dialect support, object
   managers, editor/configurator pages, connection/auth/network UI, value
   managers, and operational integrations.

## Findings Matrix

### F-001 (High): ScratchBird needs a DBeaver datasource provider plugin, not just a JDBC JAR

- Requirement intent:
  - DBeaver database ecosystems are registered as datasource providers with
    driver descriptors and navigator metadata.
- Observed behavior:
  - DBeaver exposes the `org.jkiss.dbeaver.dataSourceProvider` extension point,
    and the ScratchBird adapter contributes a datasource named `scratchbird`
    with driver class `com.scratchbird.jdbc.SBDriver`.
- Evidence:
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.registry/plugin.xml:10`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.registry/schema/org.jkiss.dbeaver.dataSourceProvider.exsd:6`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/plugins/org.jkiss.dbeaver.ext.scratchbird/plugin.xml:13`
- Impact:
  - Shipping only the ScratchBird JDBC driver would allow generic/manual
    configuration at best, but not a first-class DBeaver ecosystem entry.

### F-002 (High): Recursive schema directories are implemented through navigator tree injection

- Requirement intent:
  - Dotted ScratchBird schemas such as `users.alice.dev` must appear as nested
    directory-style nodes in the DBeaver navigator.
- Observed behavior:
  - The adapter removes the inherited `generic/catalog/schema` node and injects
    a new schema tree with `property="schemaTree"`,
    `property="childSchemas"`, and `recursive=".."`.
  - DBeaver resolves recursive navigator items through `DBXTreeNode`, which
    walks parent links when `recursive=".."` is declared.
- Evidence:
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/plugins/org.jkiss.dbeaver.ext.scratchbird/plugin.xml:24`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.registry/src/org/jkiss/dbeaver/registry/DataSourceProviderDescriptor.java:598`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.model/src/org/jkiss/dbeaver/model/navigator/meta/DBXTreeNode.java:92`
- Impact:
  - "Recursive directories" are a navigator-tree concern. No separate DBeaver
    filesystem plugin is needed for this feature.

### F-003 (High): ScratchBird needs a custom generic meta model binding to its JDBC driver class

- Requirement intent:
  - The adapter must customize catalog/schema behavior while preserving DBeaver
    generic JDBC table/view loading.
- Observed behavior:
  - DBeaver resolves generic meta models by driver class.
  - The ScratchBird adapter registers `ScratchBirdMetaModel` against
    `com.scratchbird.jdbc.SBDriver`, creates `ScratchBirdDataSource` and
    `ScratchBirdCatalog`, and suppresses catalog use in object names.
- Evidence:
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.generic/src/org/jkiss/dbeaver/ext/generic/GenericMetaModelRegistry.java:83`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/plugins/org.jkiss.dbeaver.ext.scratchbird/plugin.xml:6`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/plugins/org.jkiss.dbeaver.ext.scratchbird/src/org/jkiss/dbeaver/ext/scratchbird/model/ScratchBirdMetaModel.java:29`
- Impact:
  - A pure generic-driver descriptor would not be enough to model ScratchBird's
    schema hierarchy correctly.

### F-004 (High): Recursive hierarchy is built client-side from terminal schemas

- Requirement intent:
  - Parent schema segments must exist in the navigator even if JDBC only
    returns leaf schemas.
- Observed behavior:
  - `ScratchBirdCatalog` reads the flat schema list, `ScratchBirdSchemaTreeBuilder`
    inflates dotted paths into a tree, and `ScratchBirdSchemaNode` exposes
    `getChildSchemas()` plus generic table children.
- Evidence:
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/plugins/org.jkiss.dbeaver.ext.scratchbird/src/org/jkiss/dbeaver/ext/scratchbird/model/ScratchBirdCatalog.java:35`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/plugins/org.jkiss.dbeaver.ext.scratchbird/src/org/jkiss/dbeaver/ext/scratchbird/model/ScratchBirdSchemaTreeBuilder.java:26`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/plugins/org.jkiss.dbeaver.ext.scratchbird/src/org/jkiss/dbeaver/ext/scratchbird/model/ScratchBirdSchemaNode.java:57`
- Impact:
  - The DBeaver adapter does not require JDBC to emit parent rows in order to
    render nested schema directories correctly.

### F-005 (Medium): JDBC metadata parent expansion is optional, not the primary DBeaver mechanism

- Requirement intent:
  - Some downstream clients may prefer `DatabaseMetaData.getSchemas()` to emit
    parent segments as explicit rows.
- Observed behavior:
  - ScratchBird JDBC exposes `metadataExpandSchemaParents` and related aliases.
  - `SBDatabaseMetaData.getSchemas()` appends dotted parent segments only when
    that property is enabled.
- Evidence:
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/p3/drivers/jdbc/src/main/java/com/scratchbird/jdbc/SBConnectionProperties.java:53`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/p3/drivers/jdbc/src/main/java/com/scratchbird/jdbc/SBDatabaseMetaData.java:1009`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/README.md:19`
- Impact:
  - This property is useful for compatibility experiments, but the DBeaver
    adapter itself already reconstructs the hierarchy client-side.

### F-006 (Medium): A production-ready DBeaver adapter needs plugin, feature, repository, test, and installer assets

- Requirement intent:
  - ScratchBird support must work in both stock DBeaver installs and DBeaver
    source checkouts.
- Observed behavior:
  - The integration module already contains:
    - an Eclipse plugin
    - a p2 feature
    - a p2 repository
    - a DBeaver test plugin
    - scripts for update-site build and source-checkout installation
- Evidence:
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/pom.xml:1`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/features/org.jkiss.dbeaver.ext.scratchbird.feature/feature.xml:1`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/repository/category.xml:1`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/test/org.jkiss.dbeaver.ext.scratchbird.test/src/org/jkiss/dbeaver/ext/scratchbird/model/ScratchBirdIntegrationTest.java:1`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/scripts/build-p2-update-site.sh:1`
- Impact:
  - The correct unit of delivery is the full integration module, not just the
    plugin source tree.

### F-007 (Medium): The current local DBeaver checkout is a staging area, not the source of truth

- Requirement intent:
  - Support files should live in `ScratchBird-driver` and be copied into a
    DBeaver checkout only when needed.
- Observed behavior:
  - `ScratchBird-driver` contains tracked integration files.
  - The local `dbeaver` clone has untracked ScratchBird plugin/test folders and
    modified reactor/feature files.
  - Plugin/test sources match the tracked integration module aside from the
    DBeaver-specific POM parent path.
- Evidence:
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/README.md:1`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.scratchbird/plugin.xml:1`
  - `/home/dcalford/CliWork/dbeaver/test/org.jkiss.dbeaver.ext.scratchbird.test/src/org/jkiss/dbeaver/ext/scratchbird/model/ScratchBirdIntegrationTest.java:1`
- Impact:
  - `ScratchBird-driver` should remain the canonical home for adapter sources;
    the DBeaver clone should be treated as a verification target.

### F-008 (Medium): The installer needed semantic idempotency, not literal-line idempotency

- Requirement intent:
  - Re-running the source-checkout installer must not duplicate reactor or
    feature entries if the target file already contains the ScratchBird module.
- Observed behavior:
  - The local `dbeaver` checkout contains two ScratchBird plugin lines in
    `features/org.jkiss.dbeaver.db.feature/feature.xml`, caused by whitespace
    variation defeating the install script's original literal-string check.
  - The integration installer in `ScratchBird-driver` has now been tightened to
    detect existing entries by regex instead of exact line text.
- Evidence:
  - `/home/dcalford/CliWork/dbeaver/features/org.jkiss.dbeaver.db.feature/feature.xml`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/scripts/install-into-dbeaver.sh:27`
- Impact:
  - The support module can now be used as a repeatable source-checkout seeding
    tool without reintroducing duplicate feature/module entries.

### F-009 (High): "Full native DBeaver support" is still broader than the current adapter scope

- Requirement intent:
  - A fully native-feeling DBeaver ecosystem adapter may include dialect-aware
    SQL behavior, custom editors, object managers, icons, and UI extensions.
- Observed behavior:
  - The current adapter contributes `dataSourceProvider` and `generic.meta`
    only.
  - It does not yet contribute `org.jkiss.dbeaver.sqlDialect`,
    `org.jkiss.dbeaver.objectManager`, or a companion `.ui` plugin.
- Evidence:
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/plugins/org.jkiss.dbeaver.ext.scratchbird/plugin.xml:1`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.firebird/plugin.xml:52`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.sqlite/plugin.xml:89`
- Impact:
  - Current status is "connect and browse with recursive schema support", not
    yet "feature-complete native DBeaver experience".

### F-010 (High): Per-database SQL autocomplete requires a ScratchBird SQL dialect layer

- Requirement intent:
  - DBeaver SQL completion, formatting, and editor behavior should be aware of
    ScratchBird syntax and keywords on a per-database basis.
- Observed behavior:
  - Mature DBeaver integrations contribute `org.jkiss.dbeaver.sqlDialect`.
  - MySQL and Oracle also register `org.eclipse.core.runtime.adapters` factories
    from their dialect classes to `TPRuleProvider`, which DBeaver uses for
    text/parser rule binding.
  - The current ScratchBird plugin contributes neither `sqlDialect` nor a
    dialect adapter factory.
- Evidence:
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.mysql/plugin.xml:419`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.mysql/plugin.xml:435`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.oracle/plugin.xml:493`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/plugins/org.jkiss.dbeaver.ext.scratchbird/plugin.xml:1`
- Impact:
  - ScratchBird currently falls back toward generic SQL-editor behavior, so
    autocomplete and SQL tooling will be incomplete until a ScratchBird dialect
    layer is added.

### F-011 (High): Full object lifecycle support requires object managers plus UI editors/configurators

- Requirement intent:
  - DBeaver should support database-native create/edit/drop flows and object
    source/DDL pages for ScratchBird objects.
- Observed behavior:
  - Mature integrations register non-UI object lifecycle handlers under
    `org.jkiss.dbeaver.objectManager`.
  - They pair those managers with `org.jkiss.dbeaver.databaseEditor`
    contributions and configurators in companion UI plugins.
  - The current ScratchBird adapter has neither object managers nor a UI plugin.
- Evidence:
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.firebird/plugin.xml:60`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.mysql/plugin.xml:323`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.postgresql.ui/plugin.xml:57`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.oracle.ui/plugin.xml:176`
- Impact:
  - Without this layer, ScratchBird remains browse-oriented rather than a full
    DBeaver editing target.

### F-012 (High): A complete implementation needs a dedicated ScratchBird UI plugin

- Requirement intent:
  - Database-specific connection pages, session/lock/admin views, and property
    configurators should live in a DBeaver UI module, following built-in plugin
    patterns.
- Observed behavior:
  - MySQL, PostgreSQL, and Oracle all split core model/plugin work from
    companion `.ui` plugins.
  - Those UI plugins contribute `dataSourceView`, `editorContribution`,
    `databaseEditor`, `dataManager`, `tools`, `task.ui`, and
    `ui.propertyConfigurator`.
  - ScratchBird currently has only a core plugin.
- Evidence:
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.mysql.ui/plugin.xml:78`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.postgresql.ui/plugin.xml:6`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.oracle.ui/plugin.xml:158`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/integrations/scratchbird-dbeaver-driver/plugins/org.jkiss.dbeaver.ext.scratchbird/plugin.xml:1`
- Impact:
  - A full implementation should plan for at least two plugin modules:
    ScratchBird core and ScratchBird UI.

### F-013 (Medium): Connection/auth/network UX requires explicit DBeaver extension coverage

- Requirement intent:
  - ScratchBird-specific authentication and TLS/network settings should be
    surfaced cleanly in DBeaver connection creation and edit flows.
- Observed behavior:
  - Mature integrations use `dataSourceView` for wizard/editor pages,
    `networkHandler` for SSL and related properties, `dataSourceAuth` for
    alternative auth models, and `ui.propertyConfigurator` for corresponding UI.
  - The current ScratchBird adapter relies on the generic connection path.
- Evidence:
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.postgresql/plugin.xml:739`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.postgresql/plugin.xml:774`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.postgresql.ui/plugin.xml:182`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.mysql.ui/plugin.xml:86`
- Impact:
  - ScratchBird-specific auth and secure-transport UX remains under-specified in
    DBeaver until these extension points are mapped and implemented.

### F-014 (Medium): Rich type handling and data editing require data-type and data-manager plugins

- Requirement intent:
  - ScratchBird-specific or nuanced JDBC types should have correct rendering,
    editing, and specialized value managers in DBeaver.
- Observed behavior:
  - Mature integrations register `dataTypeProvider` in core plugins and
    `dataManager` handlers in UI plugins for type-specific editing and display.
  - ScratchBird does not yet register either extension point.
- Evidence:
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.mysql/plugin.xml:338`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.postgresql/plugin.xml:706`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.postgresql.ui/plugin.xml:159`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.oracle/plugin.xml:424`
- Impact:
  - Even with correct JDBC transport, DBeaver value rendering/editing will not
    be complete for ScratchBird-specific type behavior.

### F-015 (Medium): Operational parity requires tasks, tools, dashboards, and related integrations where ScratchBird supports them

- Requirement intent:
  - A complete DBeaver integration should expose supported operational features
    such as backup, script execution, maintenance actions, dashboards, SQL
    generators, and native-client tasks where ScratchBird has corresponding
    capabilities.
- Observed behavior:
  - Mature database plugins contribute `task`, `tools`, `task.ui`,
    `dashboard`, `sqlGenerator`, `sqlCommand`, `sqlBackup`, and sometimes
    native-client packaging or Maven repository metadata.
  - ScratchBird currently contributes none of these DBeaver feature families.
- Evidence:
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.mysql/plugin.xml:409`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.mysql/plugin.xml:440`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.mysql/plugin.xml:481`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.postgresql/plugin.xml:785`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.postgresql/plugin.xml:851`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.postgresql/plugin.xml:886`
  - `/home/dcalford/CliWork/dbeaver/plugins/org.jkiss.dbeaver.ext.postgresql/plugin.xml:901`
- Impact:
  - "Complete driver implementation" must explicitly decide which operational
    feature families ScratchBird supports and wire each supported family into
    DBeaver.

## Required Next Decisions

1. Treat `tracks/alpha/integrations/scratchbird-dbeaver-driver` as the only
   source of truth for adapter files.
2. Treat the current adapter as a foundation layer only, not the finished
   ScratchBird DBeaver implementation.
3. Decide the exact ScratchBird object/tool feature set that should map into:
   - SQL editor/dialect support
   - object managers and editors
   - auth/network UI
   - tasks/tools/dashboards/native clients
4. Decide whether the local `~/CliWork/dbeaver` checkout should be cleaned and
   normalized after the current staging work, or left as-is until active
   implementation resumes.
