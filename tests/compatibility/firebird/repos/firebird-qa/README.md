# Firebird QA

This repository contains:

- pytest plugin that provides support for testing the Firebird engine. It uses new Python
  driver for Firebird (`firebird-driver`).
- tests for Firebird engine (directory `tests`)
- files needed by tests (directories `databases`, `files`, `backups` and `configs`)

**Requirements:** Python 3.8+, Firebird 3+

You should definitelly read the [QA suite documentation](https://firebird-qa.readthedocs.io)!

## Quickstart

1. Clone the git repository

2. If you don't have `pipx` tool installed, install it using::

   ```
   python -m pip install pipx
   ```

   Or you can use any other method listed at [pipx website](https://pipx.pypa.io).

   > Don't forget to run:
   >
   >  pipx ensurepath
   >
   > once after installation to ensure that tools installed via `pipx` will be available on
   > search path.

3. Install the plugin and required dependencies by running next command:

   ```
   pipx install --include-deps firebird-qa
   ```

3. Adjust Firebird server configuration.

   **ONLY FOR MANUAL runs:**

   Check content of `$FB_HOME/databases.conf.`

   Ensure that RemoteAccess is allowed for `security.db`.
   Also, it is recommended to set number of buffers not less than 256 for it:

   ```
   security.db = $(dir_secDb)/security<suffix>.fdb
   {
       RemoteAccess = true
       DefaultDbCachePages = 256
   }
   ```

   This must be done only if you want to run some tests manually.
   Automated scenario for running tests will overwrite this file
   and put there all needed data before every pytest session (using
   `$QA_ROOT/files/qa-databases.conf` as prototype for that purpose).

    1. `$FB_HOME/firebird.conf`:

        **Firebird 3:**

        ```
        # Required
        BugcheckAbort = 1
        ExternalFileAccess = Full
        AuthServer = Srp, Win_Sspi, Legacy_Auth
        UserManager = Srp, Legacy_UserManager
        WireCrypt = Enabled
        FileSystemCacheThreshold = 99999K
        IpcName = xnet_fb3x_qa
        RemotePipeName = wnet_fb3x_qa

        # Recommended
        DefaultDbCachePages = 10000
        MaxUnflushedWrites = -1
        MaxUnflushedWriteTime = -1

        # Needed for encryption-related tests.
        KeyHolderPlugin = fbSampleKeyHolder
        ```

        **Firebird 4:**
        ```
        # Required
        BugcheckAbort = 1
        ExternalFileAccess = Full
        AuthServer = Srp, Win_Sspi, Legacy_Auth
        UserManager = Srp, Legacy_UserManager
        ReadConsistency = 0
        WireCrypt = Enabled
        ExtConnPoolSize = 10
        ExtConnPoolLifeTime = 10
        UseFileSystemCache = true
        IpcName = xnet_fb4x_qa
        RemotePipeName = wnet_fb4x_qa

        # Recommended
        DefaultDbCachePages = 10000
        MaxUnflushedWrites = -1
        MaxUnflushedWriteTime = -1

        # number of seconds after which statement execution will be automatically cancelled
        # by the engine
        # can be very useful if some test will hang or become work extremely slow:
        StatementTimeout = 7200

        # Needed for encryption-related tests:
        KeyHolderPlugin = fbSampleKeyHolder
        ```

        **Firebird 5:**

        currently all parameters from FB-4.x can be used, except 'RemotePipeName'
        because support of WNET protocol was removed from FB-5.x.
        It is recommended to assign value like 'xnet_fb5x_qa' to IpcName.

        > [!NOTE]
        > Proper values of some parameters strongly depends on ServerMode and amount of
        > avaliable RAM.
        >
        > * DefaultDbCachePages::
        >   On Classic/SuperClassic it must not be greater than 4K in real systems. For tests
        >   10K...20K is OK.
        >   On Super it can be increased so that size of page cache become 25%...33% of
        >   physical RAM.
        > * TempCacheLimit::
        >   On Classic usually it must be limited because every connection uses its own memory area
        >   for sort purposes. Nowadays may to use values about 256M ... 512M.
        >   On SuperClassic and Super common memory area is used to sorts, so this parameter can have
        >   values of dozen Gb. As first approximation, it can be set up to 33% of total RAM.


    2. Changes in OS environment variables:

       it is recommended to create variable `FIREBIRD_TMP` that will point to the pre-created directory
       on some fast drive (e.g. SSD or RAM). This drive must have at least 30 Gb free space.
       Once this variable is defined, one may not specify parameter 'TempDirectories'.

    3. Required changes for running encryption-related tests::

        1. Change dir to `$FB_HOME/examples/prebuilt/plugins/` and make copy of following files into `$FB_HOME/plugins/` :

            1. Configs:

               ```
               fbSampleKeyHolder.conf
               fbSampleDbCrypt.conf
               ```
            2. Binaries on Windows:
               ```
               fbSampleDbCrypt.dll
               fbSampleKeyHolder.dll
               ```
            3. Binaries on Linux:
               ```
               libfbSampleDbCrypt.so
               libfbSampleKeyHolder.so
               ```

               > [!NOTE]
               > These files missed in Firebird 3.x but one may to use such files from any
               > recent FB 4.x snapshot.
               > Config parameter KeyHolderPlugin currently is 'fbSampleKeyHolder'.
               > This value must match to value of parameter 'ENCRYPTION_HOLDER' that is
               > specified in the file `$(QA_ROOT)/files/test_config.ini` (it contains several settings that are common for many tests).

        2. In `$FB_HOME/plugins/fbSampleKeyHolder.conf`:
           ```
           Auto = true
           KeyRed=111
           KeyGreen = 119
           ```
        3. In `$FB_HOME/plugins/fbSampleDbCrypt.conf`:
           ```
           # Encure that Auto = false or just is commented out
           ```
        4. Restart Firebird and check that all set correct. Example for Linux:
           ```
           shell rm -f /var/tmp/tmp4test.fdb;
           create database 'localhost:/var/tmp/tmp4test.fdb' user sysdba password 'masterkey';
           ```

           **Following must PASS:**
           ```
           set echo on;
           set bail on;
           alter database encrypt with "fbSampleDbCrypt" key Red;
           shell sleep 2;

           alter database decrypt;
           shell sleep 2;

           alter database encrypt with "fbSampleDbCrypt" key Green;
           shell sleep 2;

           alter database decrypt;
           shell sleep 2;

           set echo off;
           set bail off;
           ```
           **Following must FAIL with:**
           ```
           -- Statement failed, SQLSTATE = 42000
           -- unsuccessful metadata update
           -- -ALTER DATABASE failed
           -- -Missing correct crypt key
           -- -Plugin fbSampleKeyHolder:
           -- -Crypt key NOSUCH not set

           set echo on;
           alter database encrypt with "fbSampleDbCrypt" key NoSuch;
           shell sleep 2;

           show database;
           quit;
           ```
        > [!IMPORTANT]
        > Ensure that EMPLOYEE database was not encrypted before with key/value that is unknown currently!
        > Otherwise attempt to run ANY test will fail with:
        > ```
        >   INTERNALERROR> firebird.driver.types.DatabaseError: Missing database encryption key for your attachment
        >   INTERNALERROR> -Plugin fbSampleKeyHolder:
        >   INTERNALERROR> -Crypt key <HERE_SOME_UNKNOWN_KEY> not set
        > ```

    4. Additional issues about folder `$(dir_sampleDb)` (`$FB_HOME/examples/empbuild/`) and its subdirectories.

        1. There are many tests which supposes that this directory is avaliable for read/write access.
           Test suite (firebird-qa plugin for pytest) will re-create subdirectory with name 'qa' under `$(dir_sample)` for
           every such test, so be sure that you have not any significant data in this folder.
        2. Firebird 4.x+ has ability to involve databases in replication schema. There are several tests which assumes that
           such schema already was created (before pytest session) and there arte two databases in it (master and replica).
           It was decided to use directory with name: `$(dir_sampleDb)/qa_replication/` for this purpoces. Two databases must
           be created in it: `db_main.fdb` and `db_repl.fdb`, and one need to prepare them into replication beforehand.
           Also, one need to prepare two directories in THIS folder which will serve as replication journal and archive.
           All these actions are performed by batch scenarios which can be provided by IBSurgeon company by request.


4. Optional. Enable your OS to create dump files in case of FB crashes caused by tests::

    1. Windows:
        1. Parameter `BugcheckAbort` must always be set to 1, otherwise dumps will not be created.
        2. Run regedit, navigate to key:
           ```
           HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\
           ```
           Create sub-key there with name: 'firebird.exe' (without single quotes).
           Add following parameters in the 'firebird.exe' key:
           ```
           DumpCount, type = DWORD, value: not less than 5;
           DumpFoler, type = REG_EXPAND_SZ, value = directory where you want dumps to be created;
           DumpType, type = DWORD, value = 2
           ```
        3. Following setting must present in the registry to disable any pop-up window when program crashes:
           key: `HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\Windows Error Reporting\`
           parameter: `'DontShowUI', type =  DWORD, value: 2`

    4.2. Linux:
        File `/etc/security/limits.conf` must have setting:
        ```
            *               soft    core            unlimited
        ```
        File `/etc/sysctl.conf` must have parameter `kernel.core_pattern` that specifies
        directory to store dumps and pattern for dumps name, e.g.:
        ```
        kernel.core_pattern=/var/tmp/core.%e.%t.%p
        ```

5. Cautions.
    1. Problems can occur on Windows if we launch two FB instances which uses the same major version ODS.
       Currently this relates to FB-4.x and FB-5.x: each of them tries to create file 'fb13_user_mapping'
       in `%programdata%\firebird`. This leads to conflict and attempt to connect to any DB using latter FB instance
       issues "Error occurred during login, please check server firebird.log for details" and firebird.log will have:
       "Database is probably already opened by another engine instance in another Windows session".
       BE SURE THAT YOU DID NOT LAUNCH ANOTHER FIREBIRD INSTANCE THAT USES SAME ODS AS CURRENTLY TESTING.
    2. Be sure that directory specified by FIREBIRD_TMP variable actually exists and is accessible for 'firebird' account.
    3. Ensure that your `firebird-driver.conf` contains 'DEFAULT' section with `encoding_errors = ignore`.
       Otherwise outcome of some tests can be unpredictable if your OS has non-ascii system console

6. Use pytest to run tests.

   The plugin adds next options to pytest:
   ```
        Firebird server:
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
   To run all tests (except slow ones) against local server use next command::
   ```
   pytest --server local ./tests
   ```

   > [!NOTE]
   > If plugin fails to determine the directory with Firebird utilities (isql, gbak etc.),
   > use `--bin-dir` option to specify it.
