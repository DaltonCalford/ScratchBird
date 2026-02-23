#coding:utf-8

"""
ID:          issue-7868
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/7868
TITLE:       SET AUTOTERM ON/OFF. Ability to handle long DDL in one line without 'set term'.
DESCRIPTION: DDL was taken from test for core-4882 (DDL of OLTP-EMUL transformed to one-line).
NOTES:
    Test verifies that one may to compile long DDL which is written in one line without 'SET TERM' statements.
    Also, we check that this DDL can be compiled using '-autot' command switch for ISQL (rather than command 'SET AUTOTERM' inside .sql)

    Checked on 6.0.0.139 (intermediate snapshot of 23-nov-2023).
"""

from zipfile import Path
import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', substitutions=[('[ \t]+',' '), ('CURRENT_TIMESTAMP .*', '')] )

#--------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    script_file = Path(act.files_dir / 'autoterm_long_ddl_in_one_line.zip', at='autoterm_long_ddl_in_one_line.sql')
    test_sql = script_file.read_text()
    
    act.expected_stdout = """
        MSG AUTOTERM: oltp30_DDL.sql start
        MSG AUTOTERM: oltp30_DDL.sql finish
        MSG AUTOTERM: oltp30_sp.sql start
        MSG AUTOTERM: oltp30_sp.sql finish
        MSG AUTOTERM: oltp_main_filling.sql start
        MSG AUTOTERM: oltp_main_filling.sql finish
    """
    act.isql(switches=['-q', '-autot'], input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
