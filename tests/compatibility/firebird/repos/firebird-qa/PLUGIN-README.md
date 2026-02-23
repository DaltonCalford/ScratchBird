# pytest plugin for Firebird QA

## Installation

If you plan to use this plugin for personal purposes (not related to Firebird project QA),
we recommend to use `pipx` to install `pytest` together with `firebird-qa` plugin:

```
pipx --include-deps firebird-qa
```

## Configuration

### Firebird-driver configuration

The QA plugin uses firebird-driver to access the Firebird servers, and uses driver configuration
object to set up various driver and server/database connection parameters. The configuration object
is initialized from `firebird-driver.conf` file, and plugin specifically utilizes server sections
in this file. When pytest is invoked, you must specify tested server with `–server <name>` option,
where `<name>` is name of server configuration section in `firebird-driver.conf` file.

This file is stored in firebird-qa repository, and defines default configuration suitable to most QA setups.

Note:

The `firebird-driver.conf` file should be located in QA root directory. In default setup, the QA plugin
is used to test local Firebird installation with default user name and password (SYSDBA/masterkey)
via local server (configuration section).

Important:

The firebird-driver currently does not support specification of client library in server sections.
However, the QA plugin works around that limitation. If server section for tested server contains
`fb_client_library` option specification, it’s copied to global setting.

See configuration chapter in [driver documentation](https://firebird-driver.readthedocs.io) for details.

### Pytest configuration

While it’s not required, it’s recommended to create pytest configuration file in QA root directory.
You may use this file to simplify your use of pytest with addopts option, or adjust pytest behaviour.

Suggested options for `pytest.ini`:
```
console_output_style = count
testpaths = tests
addopts = --server local --install-terminal
```

## Running test for Firebird

To run all tests in suite against local Firebird server, invoke:
```
pytest --server local ./tests
```

Tip: If you created `pytest.ini` with recommended values, you can just invoke pytest without additional parameters.

### pytest report header

When pytest is invoked, a report header is printed on terminal before individual tests are executed.
The QA plugin extend this header with next information:

- Python encodings

  - system
  - locale
  - filesystem

- Information about tested Firebird server

  - conf. section name
  - version
  - mode
  - architecture
  - home directory
  - tools directory
  - used client library

### pytest switches installed by QA plugin

The QA plugin installs several pytest command-line switches. When you run pytest ``--help``,
they are listed in Firebird QA section:
```
Firebird QA:
   --server=SERVER       Server configuration name
   --bin-dir=PATH        Path to directory with Firebird utilities
   --protocol={xnet,inet,inet4,wnet}
                         Network protocol used for database attachments
   --runslow             Run slow tests
   --save-output         Save test std[out|err] output to files
   --skip-deselected={platform,version,any}
                         SKIP tests instead deselection
   --extend-xml          Extend XML JUnit report with additional information
   --install-terminal    Use our own terminal reporter
```

**server:**

REQUIRED option. Section name in firebird-driver.conf with connection parameters for tested server.

**bin-dir:**

Normally, the QA plugin detects and properly sets the directory where Firebird tools are installed.
However, you can set this directory explicitly using the --bin-dir switch.

**protocol:**

Override for network protocol specified in firebird-driver.conf file (or default).

**runslow:**

Tests that run for longer than 10 minutes on equipment used for regular Firebird QA should be
marked as slow. They are not executed, unless this switch is specified.

**save-output:**

_Experimental switch_

When this switch is specified, stdout/stderr output of external Firebird tool executed by
test is stored in `/out` subdirectory. Intended for test debugging.

**skip-deselected:**

Tests that are not applicable to tested server (because they are for specific platform or
Firebird versions) are deselected during pytest collection phase. It means that they are not
shown in test session report. This switch changes the routine, so tests are marked to skip
(with message explaining why) instead deselection, so they show up is session report.

**extend-xml:**

When this switch is used together with `--junitxml` switch, the produced JUnitXML file will
contain additional metadata for testsuite and testcase elements recorded as property sub-elements.

  **Important:**

  Please note that using this feature will break schema verifications for the latest JUnitXML schema.
  This might be a problem when used with some CI servers.

**install-terminal:**

This option changes default pytest terminal reporter that displays pytest NODE IDs, to custom
reporter that displays Firebord QA test IDs.

pytest node IDs are of the form `module.py::class::method` or `module.py::function`.

Firebord QA test IDs are defined in our test metadata.

  **Important:**

  Right now, the custom terminal is opt-in feature. This will be changed in some future release
  to opt-out using new switch.


### Test for use with this plugin

Please read the [plugin documentation](https://firebird-qa.rtfd.io) for instructions how
to create tests that use special support provided by this plugin.
