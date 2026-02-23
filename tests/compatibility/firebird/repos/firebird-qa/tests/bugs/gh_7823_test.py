#coding:utf-8

"""
ID:          issue-7823
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7823
TITLE:       ISQL command SHOW DATABASE crashes in absence of firebird.msg
NOTES:
    [24.01.2024] pzotov
    Test implemented only for Windows.
    Confirmed bug on 6.0.0.222: ISQL crashes after 'show database' and further statements are not executed.
    Checked on 6.0.0.223 - all fine.
"""
import shutil
import subprocess
from pathlib import Path
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db', substitutions = [('^((?!(SUCCESS_MSG)).)*$', ''), ('[ \t]+', ' ')])

expected_stdout = """
    SUCCESS_MSG Ok
"""

tmp_isql = temp_file('isql.exe')
tmp_clnt = temp_file('fbclient.dll')
tmp_sql = temp_file('check.sql')
tmp_log = temp_file('check.log')

@pytest.mark.version('>=6.0')
@pytest.mark.platform('Windows')
def test_1(act: Action, tmp_isql: Path, tmp_clnt: Path, tmp_sql: Path, tmp_log: Path, capsys):
    print(Path(act.vars['bin-dir'],'isql.exe'))
    print(tmp_isql)
    shutil.copy2(Path(act.vars['bin-dir'],'isql.exe'), tmp_isql)
    shutil.copy2(Path(act.vars['bin-dir'],'fbclient.dll'), tmp_clnt)

    #cmd_isql = [str(tmp_isql), act.db.dsn, '-user', act.db.user, '-pas', act.db.password, '-i', str(tmp_sql)]
    cmd_isql = [str(tmp_isql), act.vars['host']+'/'+act.vars['port']+':'+str(act.db.db_path), '-user', act.db.user, '-pas', act.db.password, '-i', str(tmp_sql)]
    cmd_line = ' '.join(cmd_isql)
    sql_text = f"""
        -- {cmd_line}
        set list on;
        -- this cased crash of ISQL:
        show database;
        -- this was not executed before fix:
        select 'Ok' as success_msg from rdb$database;
    """
    tmp_sql.write_text(sql_text)

    with open(tmp_log, 'w') as f:
        subprocess.run( cmd_isql, stdout = f, stderr = subprocess.STDOUT)
    
    with open(tmp_log, 'r') as f:
        for line in f:
            print(line)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
