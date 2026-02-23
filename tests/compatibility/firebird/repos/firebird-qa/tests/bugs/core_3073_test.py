#coding:utf-8

"""
ID:          issue-3452
ISSUE:       3452
TITLE:       Foreign key cascade with SET DEFAULT uses the default value of the moment of the FK creation
DESCRIPTION:
JIRA:        CORE-3073
FBTEST:      bugs.core_3073
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table tdetl(x int);
    recreate table tmain(x int);
    commit;
    set term ^;
    execute block as
    begin
      execute statement 'drop domain dm_name';
      when any do begin end
    end
    ^
    set term ;^
    commit;

    create domain dm_name varchar(15) default 'Old Trafford';
    commit;

    recreate table tmain(name dm_name primary key);
    recreate table tdetl(name dm_name references tmain on delete set default);
    commit;

    alter domain dm_name set default 'New Vasyuki';
    commit;

    insert into tmain values('London');
    insert into tmain values('Old Trafford');
    insert into tmain values('New Vasyuki');

    insert into tdetl values('London');
    insert into tdetl values('Old Trafford');

    set list on;

    select 'before cascade on tdetl' as msg, d.* from tdetl d;
    delete from tmain where name = 'London';
    select 'after cascade on tdetl' as msg, d.* from tdetl d;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             before cascade on tdetl
    NAME                            London
    MSG                             before cascade on tdetl
    NAME                            Old Trafford

    MSG                             after cascade on tdetl
    NAME                            New Vasyuki
    MSG                             after cascade on tdetl
    NAME                            Old Trafford
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

