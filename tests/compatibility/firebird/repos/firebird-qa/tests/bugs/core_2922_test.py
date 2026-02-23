#coding:utf-8

"""
ID:          issue-3305
ISSUE:       3305
TITLE:       Character set used in constants is not registered as dependency
DESCRIPTION:
JIRA:        CORE-2922
FBTEST:      bugs.core_2922
"""

import pytest
from firebird.qa import *

db = db_factory(charset='utf8')

test_script = """
    set term ^;
    create or alter procedure p1 as
    declare variable a varchar(10) character set win1250;
    begin
      rdb$set_context('user_session', 'x', :a);
    end
    ^
    create or alter procedure p2 as
    begin
      post_event _win1250'abc';
    end
    ^
    set term ;^
    commit;

    -- show proc;
    set width dep_name 10;
    set width dep_on 10;
    set width dep_on_type 20;
    set list on;

    select rd.rdb$dependent_name dep_name, rd.rdb$depended_on_name dep_on,rt.rdb$type_name dep_on_type
    from rdb$dependencies rd
    join rdb$types rt on
        rd.rdb$depended_on_type = rt.rdb$type
        and rt.rdb$type_name containing upper('COLLATION')
    order by dep_name, dep_on
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    DEP_NAME                        P1
    DEP_ON                          UTF8
    DEP_ON_TYPE                     COLLATION
    DEP_NAME                        P1
    DEP_ON                          WIN1250
    DEP_ON_TYPE                     COLLATION
    DEP_NAME                        P2
    DEP_ON                          WIN1250
    DEP_ON_TYPE                     COLLATION
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

