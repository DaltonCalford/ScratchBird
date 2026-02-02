# Wiki Directory Structure

**Created:** 2026-01-03
**Last Updated:** 2026-01-30
**Purpose:** Directory structure for ScratchBird wiki documentation
**Status:** Active; content populated for Alpha documentation

---

## Current Structure (Actual)

```
wiki/
├── README.md
├── DIRECTORY_STRUCTURE.md
├── LOGO_USAGE.md
├── SETUP_COMPLETE.md
├── SYNC_STATUS.md
│
├── content/
│   ├── Home.md
│   ├── Getting-Started.md
│   ├── FAQ.md
│   ├── Contributing.md
│   ├── _Sidebar.md
│   ├── _Footer.md
│   │
│   ├── getting-started/
│   │   ├── first-connection.md
│   │   └── basic-sql.md
│   │
│   ├── developer-guide/
│   │   ├── README.md
│   │   ├── Architecture.md
│   │   ├── Core-Engine.md
│   │   ├── Storage.md
│   │   ├── Transactions.md
│   │   ├── SBLR.md
│   │   ├── Parsers.md
│   │   ├── Network-Listeners.md
│   │   ├── Security.md
│   │   └── Testing-and-Audit.md
│   │
│   ├── language-guides/
│   │   ├── README.md
│   │   ├── native/
│   │   │   ├── README.md
│   │   │   ├── 01_databases_and_schemas.md
│   │   │   ├── 02_tables_and_constraints.md
│   │   │   ├── 03_indexes_views_sequences.md
│   │   │   ├── 04_types_and_domains.md
│   │   │   ├── 05_programmable_sql.md
│   │   │   ├── 06_dml_select.md
│   │   │   ├── 07_dml_modification.md
│   │   │   ├── 08_transactions.md
│   │   │   ├── 09_security_dcl.md
│   │   │   ├── 10_session_show_set.md
│   │   │   ├── 11_utilities.md
│   │   │   ├── 12_operators.md
│   │   │   ├── 13_system_catalog.md
│   │   │   └── 14_functions.md
│   │   ├── firebirdsql/
│   │   │   ├── README.md
│   │   │   ├── 01_databases_and_schemas.md
│   │   │   ├── 02_tables_and_constraints.md
│   │   │   ├── 03_indexes_views_sequences.md
│   │   │   ├── 04_types_and_domains.md
│   │   │   ├── 05_programmable_sql.md
│   │   │   ├── 06_dml_select.md
│   │   │   ├── 07_dml_modification.md
│   │   │   ├── 08_transactions.md
│   │   │   ├── 09_security_dcl.md
│   │   │   ├── 10_session_show_set.md
│   │   │   ├── 11_utilities.md
│   │   │   ├── 12_operators.md
│   │   │   ├── 13_system_catalog.md
│   │   │   └── 14_functions.md
│   │   ├── postgresql/
│   │   │   ├── README.md
│   │   │   ├── 01_databases_and_schemas.md
│   │   │   ├── 02_tables_and_constraints.md
│   │   │   ├── 03_indexes_views_sequences.md
│   │   │   ├── 04_types_and_domains.md
│   │   │   ├── 05_programmable_sql.md
│   │   │   ├── 06_dml_select.md
│   │   │   ├── 07_dml_modification.md
│   │   │   ├── 08_transactions.md
│   │   │   ├── 09_security_dcl.md
│   │   │   ├── 10_session_show_set.md
│   │   │   ├── 11_utilities.md
│   │   │   ├── 12_operators.md
│   │   │   ├── 13_system_catalog.md
│   │   │   └── 14_functions.md
│   │   └── mysql/
│   │       ├── README.md
│   │       ├── 01_databases_and_schemas.md
│   │       ├── 02_tables_and_constraints.md
│   │       ├── 03_indexes_views_sequences.md
│   │       ├── 04_types_and_domains.md
│   │       ├── 05_programmable_sql.md
│   │       ├── 06_dml_select.md
│   │       ├── 07_dml_modification.md
│   │       ├── 08_transactions.md
│   │       ├── 09_security_dcl.md
│   │       ├── 10_session_show_set.md
│   │       ├── 11_utilities.md
│   │       ├── 12_operators.md
│   │       ├── 13_system_catalog.md
│   │       └── 14_functions.md
│   │
│   ├── cli-tools/
│   │   ├── README.md
│   │   ├── sb-server.md
│   │   ├── sb-isql.md
│   │   ├── sb-fb-isql.md
│   │   ├── sb-pg-isql.md
│   │   ├── sb-my-isql.md
│   │   ├── sb-security.md
│   │   ├── sb-backup.md
│   │   ├── sb-verify.md
│   │   └── sb-admin.md
│   │
│   ├── installation/
│   │   ├── README.md
│   │   ├── Docker.md
│   │   ├── Linux.md
│   │   ├── Windows.md
│   │   ├── macOS.md
│   │   ├── Kubernetes.md
│   │   ├── AppImage.md
│   │   ├── DEB-Package.md
│   │   ├── RPM-Package.md
│   │   └── Homebrew.md
│   │
│   ├── drivers/
│   │   ├── Python.md
│   │   ├── NodeJS-TypeScript.md
│   │   ├── Java-JDBC.md
│   │   ├── CSharp-DotNet.md
│   │   ├── Go.md
│   │   ├── PHP.md
│   │   ├── Pascal-Delphi.md
│   │   ├── ODBC.md
│   │   └── Driver-Comparison.md
│   │
│   ├── migration/
│   │   ├── Migration-Overview.md
│   │   ├── From-Firebird.md
│   │   ├── From-PostgreSQL.md
│   │   ├── From-MySQL.md
│   │   └── Migration-Checklist.md
│   │
│   ├── tutorials/
│   │   ├── README.md
│   │   ├── First-Application.md
│   │   ├── Web-App-Python-Flask.md
│   │   ├── Web-App-NodeJS-Express.md
│   │   ├── Desktop-App-Delphi.md
│   │   ├── REST-API-Design.md
│   │   ├── Data-Migration-Project.md
│   │   └── Docker-Deployment.md
│   │
│   ├── user-guides/
│   │   ├── README.md
│   │   ├── Transactions.md
│   │   ├── Sequences.md
│   │   ├── Indexes.md
│   │   ├── Triggers.md
│   │   ├── Trigger-Cheat-Sheet.md
│   │   ├── Procedures.md
│   │   ├── Security.md
│   │   ├── Backup-Restore.md
│   │   ├── Performance-Tuning.md
│   │   └── Vector-Search.md
│   │
│   ├── reference/
│   │   ├── SQL-Syntax.md
│   │   ├── Data-Types.md
│   │   ├── Context-Variables.md
│   │   ├── Operators.md
│   │   ├── Functions.md
│   │   ├── Error-Codes.md
│   │   └── Glossary.md
│   │
│   ├── troubleshooting/
│   │   ├── Common-Errors.md
│   │   ├── Performance-Issues.md
│   │   ├── Connection-Problems.md
│   │   ├── Installation-Issues.md
│   │   └── Debug-Guide.md
│   │
│   ├── admin/
│   │   ├── monitoring.md
│   │   ├── backup-restore.md
│   │   ├── security.md
│   │   ├── user-management.md
│   │   └── troubleshooting.md
│   │
│   ├── configuration/
│   │   ├── sb_server.conf.md
│   │   └── hba.conf.md
│   │
│   ├── connectivity/
│   │   └── postgresql-clients.md
│   │
│   └── applications/
│       └── WordPress.md
│
├── scripts/
│   └── sync-to-wiki.sh
│
├── templates/
│   ├── user-guide-template.md
│   └── driver-guide-template.md
│
├── images/
│   ├── README.md
│   ├── architecture/
│   ├── diagrams/
│   ├── screenshots/
│   └── logos/
│       ├── README.md
│       ├── TransparentScratchBirdLogoHeader.png
│       ├── ScratchBirdLogo.png
│       ├── ScratchBirdLogoHeader.png
│       └── LogoWork.svg
│
└── assets/
    ├── code-samples/
    ├── data/
    └── configs/
```

---

## File Count Summary (as of 2026-01-18)

| Category | Files | Status |
| --- | --- | --- |
| Core root content | 6 | Complete |
| Getting started (subdir) | 2 | Partial |
| Installation | 10 | Partial |
| Drivers | 9 | Partial |
| Migration | 5 | Partial |
| Tutorials | 8 | Partial |
| User guides | 11 | Active |
| Reference | 7 | Active |
| Troubleshooting | 5 | Partial |
| Developer guide | 10 | Active |
| CLI tools | 10 | Active |
| Language guides | 61 | Active |
| Admin | 5 | Active |
| Configuration | 2 | Partial |
| Connectivity | 1 | Partial |
| Applications | 1 | Partial |
| **Total content pages** | **153** | **Active** |

---

## Notes on Coverage

Remaining gaps are mostly in optional or expanded sections (additional
installation package guides, more troubleshooting pages, extra tutorials, and
additional application recipes). Use `README.md` and `_Sidebar.md` in
`content/` for current navigation.
