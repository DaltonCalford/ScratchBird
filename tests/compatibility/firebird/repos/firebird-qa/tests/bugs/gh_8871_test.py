#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8871
TITLE:       Fix locking error if IN/EXISTS is converted into a semi-join
DESCRIPTION:
NOTES:
    [28.01.2026] pzotov
    Confirmed problem on 6.0.0.1397-653b619; 5.0.4.1746-41a7ccd.
    Checked on 6.0.0.1400-b74596a; 5.0.4.1748-51d7bda
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    recreate table tmain(id int primary key);
    recreate table tdetl(id int primary key, pid int);

    insert into tmain(id) select row_number()over() from rdb$types rows 20;
    set term ^;
    execute block as
        declare n int;
        declare c int;
    begin
        select count(*) from tmain into c;
        n = c * 10;
        while (n > 0) do
        begin
            if (mod(:n, 3) > 0) then
            begin
                insert into tdetl(id, pid) values( :n, mod(:n, 3) );
            end
            n =  n - 1;
        end
    end
    ^
    set term ;^
    commit;

    select * from tmain m
    where exists (select null from tdetl d where d.pid = m.id) and id between 1 and 5
    WITH LOCK
    ;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID 1
    ID 2
"""

@pytest.mark.version('>=5.0.4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
