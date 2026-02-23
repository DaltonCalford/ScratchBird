#coding:utf-8

"""
ID:          issue-7269
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7269
TITLE:       Database restore must make every effort on activating deferred indexes
DESCRIPTION:
    Test uses unrecoverable .fbk that was provided in the ticket and tries to restore it using '-verbose' option.
    After restore finish, we check its log. It must contain SEVERAL errors related to indices (PK and two FK),
    and also it must have messages about FINAL point of restore (regardless error that follows after this):
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags
NOTES:
    [02.11.2024] pzotov
        Checked on 5.0.2.1551, 6.0.0.415.
    [04.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.863; 5.0.3.1668.
"""
import subprocess
from pathlib import Path
import zipfile
import locale
import re
import pytest
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

db = db_factory()
act = python_act('db')
tmp_fbk = temp_file('gh_7269.tmp.fbk')
tmp_fdb = temp_file('gh_7269.tmp.fdb')

@pytest.mark.version('>=5.0.2')
def test_1(act: Action, tmp_fbk: Path, tmp_fdb: Path, capsys):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_7269.zip', at = 'gh-7269-unrecoverable.fbk')
    tmp_fbk.write_bytes(zipped_fbk_file.read_bytes())

    allowed_patterns = \
    (
         r'gbak:(\s+)?ERROR(:)?'
        ,r'gbak:(\s+)?finishing, closing, and going home'
        ,r'gbak:(\s+)?adjusting the ONLINE and FORCED WRITES flags'
    )
    allowed_patterns = [ re.compile(p, re.IGNORECASE) for p in allowed_patterns ]

    act.gbak(switches = ['-rep', '-v', str(tmp_fbk), str(tmp_fdb)], combine_output = True, io_enc = locale.getpreferredencoding())

    for line in act.stdout.splitlines():
            if act.match_any(line.strip(), allowed_patterns):
                print(line)

    expected_stdout_5x = """
        gbak: ERROR:violation of PRIMARY or UNIQUE KEY constraint "PK_A3" on table "A3"
        gbak: ERROR:    Problematic key value is ("ID" = 9)
        gbak: ERROR:violation of PRIMARY or UNIQUE KEY constraint "PK_A1" on table "A1"
        gbak: ERROR:    Problematic key value is ("ID" = 5)
        gbak: ERROR:Cannot create foreign key constraint FK_A1. Partner index does not exist or is inactive.
        gbak: ERROR:violation of FOREIGN KEY constraint "FK_A2" on table "B2"
        gbak: ERROR:    Foreign key reference target does not exist
        gbak: ERROR:    Problematic key value is ("A2_ID" = 5)
        gbak: ERROR:Cannot create foreign key constraint FK_A3. Partner index does not exist or is inactive.
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags
        gbak: ERROR:Database is not online due to failure to activate one or more indices.
        gbak: ERROR:    Run gfix -online to bring database online without active indices.
    """

    expected_stdout_6x = """
        gbak: ERROR:violation of PRIMARY or UNIQUE KEY constraint "PK_A3" on table "PUBLIC"."A3"
        gbak: ERROR:    Problematic key value is ("ID" = 9)
        gbak: ERROR:violation of PRIMARY or UNIQUE KEY constraint "PK_A1" on table "PUBLIC"."A1"
        gbak: ERROR:    Problematic key value is ("ID" = 5)
        gbak: ERROR:Cannot create foreign key constraint "FK_A1". Partner index does not exist or is inactive.
        gbak: ERROR:violation of FOREIGN KEY constraint "FK_A2" on table "PUBLIC"."B2"
        gbak: ERROR:    Foreign key reference target does not exist
        gbak: ERROR:    Problematic key value is ("A2_ID" = 5)
        gbak: ERROR:Cannot create foreign key constraint "FK_A3". Partner index does not exist or is inactive.
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags
        gbak: ERROR:Database is not online due to failure to activate one or more indices.
        gbak: ERROR:    Run gfix -online to bring database online without active indices.
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
