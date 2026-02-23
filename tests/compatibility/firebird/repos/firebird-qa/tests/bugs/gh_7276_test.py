#coding:utf-8

"""
ID:          issue-7276
ISSUE:       7276
TITLE:       Firebird 4 literal containing crashes server
NOTES:
    [20.02.2023] pzotov
    Confirmed crash on 4.0.3.2824
    Checked on 4.0.3.2825 -- all fine.

    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table tbl1 ( modul varchar(10) );
    create table tbl2 ( modul varchar(10) );

    insert into tbl1 values('O7');
    insert into tbl1 values('DS');
    insert into tbl1 values('ET');
    insert into tbl1 values('T5');
    insert into tbl1 values('EF');

    insert into tbl2 values('DG');
    insert into tbl2 values('DS');
    insert into tbl2 values('EF');
    insert into tbl2 values('ET');
    insert into tbl2 values('O7');
    insert into tbl2 values('T5');

    select a.modul
    from tbl2 b
    join tbl1 a on a.modul = b.modul
    where 'TT,T0,T2,T3,T4,T5,T6,T7,T8,T9' containing b.modul
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MODUL                           T5
"""

@pytest.mark.version('>=4.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
