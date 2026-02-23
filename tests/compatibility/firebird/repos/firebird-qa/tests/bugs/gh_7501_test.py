#coding:utf-8

"""
ID:          issue-7501
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7501
TITLE:       Precision of standalone unit may differ from packaged one in SQL dialect 1
DESCRIPTION:
NOTES:
    [11.03.2023] pzotov
    Checked on 5.0.0.972; 4.0.3.2907
"""

import pytest
from firebird.qa import *

db = db_factory(sql_dialect=1)

test_script = """
    set list on;
    recreate table test(id int primary key, x numeric(9,2), y numeric(9,2));
    insert into test(id, x, y) select row_number()over(), 1.11, 333.33 from rdb$types,rdb$types rows 1000;
    commit;

    set term ^;
    create or alter function fn_sum returns double precision as
    begin
        return ( select sum(x/y) from test );
    end
    ^
    create or alter package pg as
    begin
        function pg_sum returns double precision;
    end
    ^
    recreate package body pg as
    begin
        function pg_sum returns double precision as
        begin
            return ( select sum(x/y) from test );
        end
    end
    ^
    set term ;^
    commit;

    select fn_sum(), pg.pg_sum() from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    FN_SUM                          3.330033300333080
    PG_SUM                          3.330033300333080
"""

@pytest.mark.version('>=3.0.11')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    # act.execute(combine_output=True)
    act.isql(switches=['-q', '-sql_dialect', '1'], input=test_script, combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout
