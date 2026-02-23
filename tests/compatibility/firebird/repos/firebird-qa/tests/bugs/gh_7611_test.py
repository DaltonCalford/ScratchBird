#coding:utf-8

"""
ID:          issue-7611
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7611
TITLE:       Can't backup/restore database from v3 to v4 with SEC$USER_NAME field longer than 10 characters.
DESCRIPTION: 
    Test uses .fbk which was created in FB 3.x as it is described in the ticket.
    We restore from this DB using LOCAL protocol and check that it completes OK.
NOTES:
    [04.06.2023] pzotov
    ::: NB ::: Problem can be reproduced only if we restore via LOCAL protocol rather than remote.
    Confirmed bug on 4.0.3.2949, 5.0.0.1066.
    Checked on 4.0.3.2950, 5.0.0.1067 - all fine.
"""

import zipfile
import locale
import re
from pathlib import Path
import pytest
from firebird.driver import SrvRestoreFlag
from firebird.qa import *

db = db_factory()
act = python_act('db')

fbk_file = temp_file('gh_7611.tmp.fbk')

@pytest.mark.version('>=4.0.3')
def test_1(act: Action, fbk_file: Path, capsys):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_7611.zip', at = 'gh_7611_made_in_fb_3x.fbk')
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

    expected_stdout = """
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
