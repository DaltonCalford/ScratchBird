#coding:utf-8

"""
ID:          issue-8444
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8444
TITLE:       GBAK: GDS error batch_big_seg2 when restoring a table with many BLOBs
DESCRIPTION:
NOTES:
    [28.02.2025] pzotov
    1) restore must use remote protocol, i.e. 'localhost:' must be specified in DSN of target FDB.
    2) in case of success test executes for ~60 seconds.
    Confirmed problem on 6.0.0.655-6e3e059, 5.0.3.1624-00b699c: restore terminates prematurely, gbak crashes (with retcode=3221225477).
    Checked on 6.0.0.656-25fb454
"""
from pathlib import Path
import zipfile
import locale
import re
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')
tmp_fbk = temp_file('gh_8444.tmp.fbk')
tmp_fdb = temp_file('gh_8444.tmp.fdb')

expected_stdout = """
    gbak:finishing, closing, and going home
    gbak:adjusting the ONLINE and FORCED WRITES flags
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_fbk: Path, tmp_fdb: Path, capsys):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_8444.zip', at = 'gh_8444.fbk')
    tmp_fbk.write_bytes(zipped_fbk_file.read_bytes())

    watching_patterns = \
    (
         'gbak:finishing, closing, and going home'
        ,'gbak:adjusting the ONLINE and FORCED WRITES flags'
    )
    watching_patterns = [ re.compile(p, re.IGNORECASE) for p in watching_patterns ]

    # restore _WITHOUT_ building indices:
    act.gbak(switches=['-rep', '-i', '-verbint', '1000', str(tmp_fbk), 'localhost:' + str(tmp_fdb) ], combine_output = True, io_enc = locale.getpreferredencoding())
    print(f'Restore finished with retcode = {act.return_code}')
    if act.return_code == 0:
        # Print only interesting lines from gbak tail:
        for line in act.clean_stdout.splitlines():
            for p in watching_patterns:
                if p.search(line):
                    print(line)
    else:
        # If retcode !=0 then we can print the whole output of failed gbak:
        for line in act.clean_stdout.splitlines():
            print(line)
    act.reset()

    act.expected_stdout = """
        Restore finished with retcode = 0
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
