#coding:utf-8

"""
ID:          issue-1837
ISSUE:       1837
TITLE:       Wrong current timestamp evaluation for selectable procedures
DESCRIPTION:
  In our implementation, CURRENT_DATE/TIME[STAMP] values are evaluated at the request
  (aka SQL statement) start time and are permanent for the duration of that request.
  This rule includes the nested calls (procedures and triggers) as well, i.e. they inherit
  the parent's timestamp, thus providing the stable date-time value for the entire call stack.
  However, this rule is broken for selectable procedures that evaluate current date-time
  values at every invocation.
JIRA:        CORE-1419
FBTEST:      bugs.core_1419
"""

import pytest
from firebird.qa import *

init_script = """
    create table tdelay(id int primary key);

    set term ^;
    create procedure sp_delay as
    begin
        insert into tdelay(id) values(1);
        in autonomous transaction do
        begin
            execute statement ('insert into tdelay(id) values(?)') (1);
            when any do
            begin
                -- nop --
            end
        end
        delete from tdelay where id = 1;
    end
    ^
    create procedure sp_get_timestamp returns ( ts timestamp ) as
    begin
        ts = current_timestamp;
        suspend;
    end
    ^
    create procedure sp_main returns ( ts_self timestamp, ts_execute timestamp, ts_select timestamp ) as
    begin
        -- ::: NB ::: this SP must be called in TIL with LOCK TIMEOUT <n>!
        ts_self = current_timestamp;
        execute procedure sp_get_timestamp returning_values :ts_execute;
        select ts from sp_get_timestamp into :ts_select;
        suspend;

        execute procedure sp_delay;

        ts_self = current_timestamp;
        execute procedure sp_get_timestamp returning_values :ts_execute;
        select ts from sp_get_timestamp into :ts_select;
        suspend;
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    commit;
    set transaction lock timeout 2;
    select count(*)
    from sp_main p
    where
        cast(p.ts_self as varchar(50)) = cast(p.ts_execute as varchar(50))
        and cast(p.ts_self as varchar(50)) = cast(p.ts_select as varchar(50))
    ;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    COUNT 2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

