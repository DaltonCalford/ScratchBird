#coding:utf-8

"""
ID:          issue-5903
ISSUE:       5903
TITLE:       string right truncation on restore of security db
DESCRIPTION:
JIRA:        CORE-5637
FBTEST:      bugs.core_5637
NOTES:
    [25.10.2019] pzotov
    Refactored: restored DB state must be changed to full shutdown in order to make sure tha all attachments are gone.
    Otherwise got on CS: "WindowsError: 32 The process cannot access the file because it is being used by another process".

    [14.07.2025] pzotov
    FB 6.x restore specific for backups created in FB 3.x ... 5.x: it adds in restore output messages related to migrating
    of some objects related to SRP plugin to "PLG$SRP" schema, e.g.:
    ===================
        gbak:migrating SRP plugin objects to schema "PLG$SRP"
        gbak: WARNING:error migrating SRP plugin objects to schema "PLG$SRP". Plugin objects will be in inconsistent state:
        gbak: WARNING:unsuccessful metadata update
        gbak: WARNING:    DROP VIEW "PUBLIC"."PLG$SRP_VIEW" failed
        gbak: WARNING:    DELETE operation is not allowed for system table "SYSTEM"."RDB$SECURITY_CLASSES"
        ...
    ===================
    See doc/sql.extensions/README.schemas.md, section title: '### gbak'; see 'SQL_SCHEMA_PREFIX' variable here.
    Currently these messages are suppressed.
    Checked on 6.0.0.970; 5.0.3.1668.

    [16.07.2025] pzotov
    Messages 'gbak: WARNING' no more suppressed and should be displayed if exist (and this must be considered as bug).
    Explained by Adriano, 16.07.2025 03:40.
    Fix was in fc0e37f6fd8291bc41931605c6b2c53437095ca8
    Checked on 6.0.0.1020-fc0e37f
"""

import pytest
import zipfile
from difflib import unified_diff
from pathlib import Path
from firebird.qa import *
from firebird.driver import SrvRestoreFlag, ShutdownMode, ShutdownMethod

db = db_factory()

act = python_act('db')

sec_fbk = temp_file('core5637-security3.fbk')
sec_fdb = temp_file('core5637-security3.fdb')

@pytest.mark.version('>=4.0')
def test_1(act: Action, sec_fbk: Path, sec_fdb: Path):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'core_5637.zip', at='core5637-security3.fbk')
    sec_fbk.write_bytes(zipped_fbk_file.read_bytes())
    #
    fb_log_before = act.get_firebird_log()
    # Restore security database
    with act.connect_server() as srv:
        srv.database.restore(database=sec_fdb, backup=sec_fbk, flags=SrvRestoreFlag.REPLACE)
        gbak_restore_log = srv.readlines()
        #
        fb_log_after = act.get_firebird_log()

        #
        srv.database.validate(database=sec_fdb)
        validation_log = srv.readlines()
        srv.database.shutdown(database=sec_fdb, mode=ShutdownMode.FULL, method=ShutdownMethod.FORCED, timeout=0)

    assert [line for line in gbak_restore_log if 'gbak: ERROR' in line.upper() ] == []
    assert [line for line in validation_log if 'ERROR' in line.upper()] == []
    assert list(unified_diff(fb_log_before, fb_log_after)) == []
