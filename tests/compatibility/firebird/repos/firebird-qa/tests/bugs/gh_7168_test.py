#coding:utf-8

"""
ID:          issue-7168
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7168
TITLE:       Ignore missing UDR libraries during restore
DESCRIPTION:
    Database with 3d-party UDR was created beforehand in FB 4.0.1, backed up and .fbk was compressed.
    After decompressing .fbk we try to restore this database using FB snapshot which for sure has no
    such UDR, with verbose = True.
    Test verifies that restore log contains messages about UDR and correct finish of restoring process.
NOTES:
    [02.03.2023] pzotov
    Confirmed problem on 4.0.1.2707 (21-jan-2022), 5.0.0.471 (09-apr-2022): restore fails with error
    "firebird.driver.types.DatabaseError: UDR module not loaded" and message after it that can be in localized.
    Checked on: 4.0.3.2904, 5.0.0.475 -- all OK.

    [29.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' and variable to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.1020; 5.0.3.1668.
"""

import pytest
from firebird.qa import *
import zipfile
from pathlib import Path
from firebird.driver import SrvRestoreFlag
import locale
import re

db = db_factory()

act = python_act('db')

fbk_file = temp_file('gh_7168.tmp.fbk')

@pytest.mark.version('>=4.0.1')
def test_1(act: Action, fbk_file: Path, capsys):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_7168.zip', at = 'gh_7168.fbk')
    fbk_file.write_bytes(zipped_fbk_file.read_bytes())

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
    TEST_UDR_NAME = 'CRYPTO_RSA_PRIVATE_KEY' if act.is_version('<6') else  f'{SQL_SCHEMA_PREFIX}"CRYPTO_RSA_PRIVATE_KEY"'
    expected_stdout = f"""
        gbak:restoring function {TEST_UDR_NAME}
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags
    """

    allowed_patterns = \
    (
         f'gbak:restoring function {TEST_UDR_NAME}'
        ,'gbak:finishing, closing, and going home'
        ,'gbak:adjusting the ONLINE and FORCED WRITES flags'
    )
    allowed_patterns = [ re.compile(p, re.IGNORECASE) for p in allowed_patterns ]

    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.database.restore(database=act.db.db_path, backup=fbk_file, flags=SrvRestoreFlag.REPLACE, verbose=True)
        restore_log = srv.readlines()
        for line in restore_log:
            if act.match_any(line.strip(), allowed_patterns):
                print(line)
        #assert restore_log == []

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
