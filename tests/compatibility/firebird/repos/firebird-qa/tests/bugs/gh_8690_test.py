#coding:utf-8

"""
ID:          8690
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8690
TITLE:       Check diagnostic message and errorlevel when ISQL is launched with "< nul"
DESCRIPTION:
NOTES:
    [11.09.2025] pzotov
    Test DOES NOT verify usage of long input buffer by ISQL on Windows-7.
    (see ticket title: "On Windows 7 isql exits silently right after the start.")

    Rather, it checks only that ISQL issues diagnostics message
    ("operating system directive ReadConsoleW failed") and set errorlevel=1
    when input stream is specified as "< nul".
    See comment by Vlad:
    https://github.com/FirebirdSQL/firebird/issues/8690#issuecomment-3167348529

    Confirmed issue on 6.0.0.1194; 5.0.4.1697; 4.0.7.3230 (no diag message, ISQL errolevel remained 0)
    Checked on 6.0.0.1266; 5.0.4.1704; 4.0.7.3231
"""
import subprocess
from pathlib import Path
import time
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')
tmp_bat = temp_file('tmp_gh_8690.bat')
tmp_log = temp_file('tmp_gh_8690.log')

@pytest.mark.version('>=4.0.7')
@pytest.mark.platform('Windows')
def test_1(act: Action, tmp_bat: Path, tmp_log: Path, capsys):
    
    chk_commands = f"""
        @echo off
        chcp 65001
        setlocal enabledelayedexpansion enableextensions
        {act.vars['isql']} {act.db.dsn} -user {act.db.user} -pas {act.db.password} -q < nul
        set elev=!errorlevel!
        echo ISQL errorlevel: !elev!
    """
    with open(tmp_bat, 'w') as f:
        f.write(chk_commands)
    
    with open(tmp_log, 'w') as f:
        bat_pid = subprocess.run( [tmp_bat], stdout = f, stderr = subprocess.STDOUT )
    
    with open(tmp_log, 'r') as f:
        for line in f:
            if 'errorlevel' in line or 'ReadConsole' in line:
                print(line)

    print(f'Batch retcode = {bat_pid.returncode}')

    act.expected_stdout = """
        operating system directive ReadConsoleW failed
        ISQL errorlevel: 1
        Batch retcode = 0
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
