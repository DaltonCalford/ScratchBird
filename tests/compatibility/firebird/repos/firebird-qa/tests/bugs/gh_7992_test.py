#coding:utf-8

"""
ID:          issue-7992
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7992
TITLE:       Assertion (space > 0) failure during restore
DESCRIPTION:
NOTES:
    [03.02.2024] pzotov
    Confirmed problem on 5.0.1.1328, 6.0.0.244 (common builds): restore terminates prematurely, firebird crashes.
    Checked on 5.0.1.1330, 6.0.0.247.
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
fbk_file = temp_file('gh_7992.tmp.fbk')

expected_stdout = """
    gbak:finishing, closing, and going home
    gbak:adjusting the ONLINE and FORCED WRITES flags
"""

@pytest.mark.version('>=5.0.1')
def test_1(act: Action, fbk_file: Path, capsys):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_7992.zip', at = 'gh_7992.fbk')
    fbk_file.write_bytes(zipped_fbk_file.read_bytes())

    allowed_patterns = \
    (
         'gbak:finishing, closing, and going home'
        ,'gbak:adjusting the ONLINE and FORCED WRITES flags'
    )
    allowed_patterns = [ re.compile(p, re.IGNORECASE) for p in allowed_patterns ]

    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.database.restore(database=act.db.db_path, backup=fbk_file, flags=SrvRestoreFlag.REPLACE, verbose=True)
        restore_log = srv.readlines()
        for line in restore_log:
            if act.match_any(line.strip(), allowed_patterns):
                print(line)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
