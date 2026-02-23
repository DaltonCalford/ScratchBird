# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [0.21.0] - 2025-07-20

### Changed

- Upgraded Firebird driver and base to v2.x

## [0.20.2] - 2025-06-08

### Fixed

- Fixed problem with utf8 db filenames in `Database`.

## [0.20.1] - 2025-04-29

### Changed

- Dependency on `firebird-base` changed to "~=1.8"
- Updated `hatch` configuration

## [0.20.0] - 2024-05-09

### Added

- Fixture `existing_db_factory` to directly use database from `databases` subdirectory.
  It's not intended for use in Firebird QA, but it's necessary for other plugin
  users.

### Fixed

- Report test error also in cases when unexpected stderr is returned from tool execution
  while `returncode` is zero.
- Select test marked for current platform also when it's not marked for Firebird version.

## [0.19.3] - 2024-03-21

### Fixed

- Problem with ndiff in assert

## [0.19.2] - 2024-02-20

### Fixed

- Remove fix for #21. The error was not caused by pytest 8.0, but by `Error` exception from
  `firebird-base` package that masked the absence of `__notes__` attribute from `pytest`.
  Dependency to pytest reverted to `>=7.4`.

### Changed

- Updated documentation.

## [0.19.1] - 2024-02-09

### Fixed

- Fix for #21. Dependency to pytest changed from `>=8.0.0` to `~=7.4`. Other dependecies
  changed from `>=` to `~=`.

## [0.19.0] - 2024-02-08

### Changed

- Switch from `setuptools` to `hatch`.
- Updated dependencies.

## [0.18.0] - 2023-02-14

### Added

- Added cache for empty databases. This works transparently and does not require any
  special configuration. Databases are stored in `dbcache` subdirectory (created automatically)
  for combination of ODS + page size + SQL dialect + character set.

  Files in `dbcache` directory could be removed as needed (including whole directory)
  to fore creation of new database.

  Cache is enabled by default. Use new --disable-db-cache option to disable it.

## [0.17.3] - 2023-02-14

### Added

- Added `--driver-config` option to specify different filename for driver configuration.

## [0.17.2] - 2023-01-17

### Fixed

- Trace session support in plugin now uses service query with timeout (provided by
  `firebird-driver 1.8.0`) and terminates the trace thread gracefuly even if terminating
  trace session fails.

## [0.17.1] - 2022-11-21

### Added

- When database initialization script fails, the XML output is extended with `dbinit-stderr`
  property that contains `stderr` output with errors reported by ISQL.

### Fixed

- Uregistered bug in trace.TraceConfig - redundant `flags` definition.

## [0.17.0] - 2022-06-30

### Added

- Added `Mapping` and `mapping_factory`.

### Changed

- Variable `test_cfg` renamed to `QA_GLOBALS`.

## [0.16.0] - 2022-06-19

### Added

- Added support for configuration of tests. A `configparser.ConfigParser` instance is
  available as `test_cfg`. This instance is initialized with values from file `test_config.ini`
  located in `files` subdirectory.

## [0.15.2] - 2022-06-13

### Fixed

- Fix problem with database init script. Now it uses the database charset instead default
  UTF8. The UTF8 is used only when database charset is not specified.


## [0.15.1] - 2022-06-08

### Added

- Added `encryption` marker to mark test as requiring the encryption plugin

### Changed

- Package `psutil` is now a dependency, installed automatically with plugin.

## [0.15.0] - 2022-06-05

### Added

- Added possibility to use databases aliases. The `db_factory()` parameter `filename` is
  now handled as database alias if it starts with `#`, for example `#employee` means alias
  `employee`. The alias must be defined in `databases.conf` file.

  When filename is an alias, the `Database.db_path` property does not contain
  full `pathlib.Path` to the database, but this database alias.

### Changed

- To simplify portable use of databases with special configuration via `databases.conf`,
  the plugin initialization now ensures empty subdirectory `QA` in Firebird sample directory.
  To define your test databases in `databases.conf`, use next pattern:

  ```
    my_db = $(dir_sampleDB)/QA/my-db.fdb
    {
    ...
    }
  ```

  On plugin initialization, the `QA` sub-directory is first emptied and removed, and then
  newly created. On non-Windows, full privileges are granted.


## [0.14.0] - 2022-05-12

### Added

- Added possibility to specify user, password and role in `Action.connect_server()` and
  `.Action.trace()`

### Changed

- DataList is now generic class.
- DataList.extract() has new 'copy' argument.

## [0.13.1] - 2022-05-12

### Fixed

- Fixed problem with service encoding
- Fixed problem with tags in User

## [0.13.0] - 2022-04-19

### Added

- Explicit `Optional` typing.
- Added support for both `encoding` and `encoding_errors` in `Action.connect_server()`
  and `Action.trace()`.

## [0.12.1] - 2022-02-24

Initial release.

