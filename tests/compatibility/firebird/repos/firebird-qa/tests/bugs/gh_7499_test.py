#coding:utf-8

"""
ID:          issue-7499
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7499
TITLE:       Problem with restore when backup has procedure which contains explicit PLAN clause with inappropriate indices.
DESCRIPTION: 
    Test uses .fbk which was created on the basis of DDL from gh_7517_test.py
    in FB 3.0.10.33665. There is stored procedure which has query that uses
    inappropriate index.
    Restore from this .fbk must finish with WARNING and display name of that index.
NOTES:
    [30.03.2023] pzotov
        Unfortunately, I could not find DDL that leads not only to warning (on FB builds after fix)
        but also to "gbak: ERROR" if we try to restore from this .fbk on FB builds *before* this
        problem was fixed: 'gbak -rep' on all major FB just silently finished w/o any message.
    [04.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' and variables - to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.909; 5.0.3.1668; 4.0.6.3214.
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

fbk_file = temp_file('gh_7499.tmp.fbk')

@pytest.mark.version('>=3.0.11')
def test_1(act: Action, fbk_file: Path, capsys):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_7499.zip', at = 'gh_7499.fbk')
    fbk_file.write_bytes(zipped_fbk_file.read_bytes())

    allowed_patterns = \
    (
         'gbak: WARNING:index \\S+ cannot be used in the specified plan'
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


    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    INDEX_NAME = 'T2_FLD' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"T2_FLD"'
    expected_stdout = f"""
        gbak: WARNING:index {INDEX_NAME} cannot be used in the specified plan
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags
    """
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
