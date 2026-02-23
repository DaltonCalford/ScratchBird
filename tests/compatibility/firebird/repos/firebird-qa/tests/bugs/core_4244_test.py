#coding:utf-8

"""
ID:          issue-4568
ISSUE:       4568
TITLE:       Problem with creation procedure which contain adding text in DOS864 charset
DESCRIPTION:
JIRA:        CORE-4244
FBTEST:      bugs.core_4244
NOTES:
    [29.06.2025] pzotov
    Confirmed bug on 2.1.7.18553.
    Replaced 'SHOW' command with query to RDB tables.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

PROC_DDL = """
        declare char_one_byte char(1) character set dos864;
        declare str varchar(1000) character set dos864;
    begin
        char_one_byte='A';
        str='B';
        str=str||char_one_byte;
    end
"""
test_script = f"""
    set list on;
    set count on;
    set term ^;
    create or alter procedure sp_test as
    {PROC_DDL}
    ^
    set term ;^
    commit;
    select p.rdb$procedure_source as blob_id from rdb$procedures p where p.rdb$procedure_name = upper('sp_test');
"""

substitutions = [('BLOB_ID.*', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = f"""
    {PROC_DDL}
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
