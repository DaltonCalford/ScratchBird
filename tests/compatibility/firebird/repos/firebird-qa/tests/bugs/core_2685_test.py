#coding:utf-8

"""
ID:          issue-3088
ISSUE:       3088
TITLE:       System triggers on view with check option are not removed
DESCRIPTION:
JIRA:        CORE-2685
FBTEST:      bugs.core_2685
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    -- Updated code: made STDOUT independent of Firebird version
    -- Note about SIMILAR TO: one need here to specify backslash TWICE inside escape clause,
    -- e.g: ... escape '\\' (fbt_run feature ?)
    set list on;
    set width dep_name1 10;
    set width dep_name2 10;
    set width dep_name3 10;
    set width dep_name4 10;

    create or alter view v1 as select 1 id from rdb$database;
    commit;
    recreate table test_table (id integer, caption varchar(10));

    ------ block 1 -----

    create or alter view v1 as
    select id, caption from test_table where id > 0;
    commit;

    select upper(left(rdb$dependent_name,5)) dep_name1
    from rdb$database b
    left join rdb$dependencies d on
        d.rdb$depended_on_name = upper( 'v1' )
        and trim(d.rdb$dependent_name) similar to upper('check\\_[[:digit:]]+') escape '\\'
    ;

    ------ block 2 -----

    create or alter view v1 as select id ,caption from test_table where id > 0
    with check option;
    commit;

    select upper(left(rdb$dependent_name,5)) dep_name2
    from rdb$database
    left join rdb$dependencies d on
        d.rdb$depended_on_name = upper( 'v1' )
        and trim(d.rdb$dependent_name) similar to upper('check\\_[[:digit:]]+') escape '\\'
    ;
    ------ block 3 -----

    create or alter view v1 as select id, caption from test_table where id > 0
    with check option;

    create or alter view v1 as select id, caption from test_table where id > 0
    with check option;

    create or alter view v1 as select id,caption from test_table where id > 0
    with check option;
    commit;
    select upper(left(rdb$dependent_name,5)) dep_name3
    from rdb$database
    left join rdb$dependencies d on
        d.rdb$depended_on_name = upper( 'v1' )
        and trim(d.rdb$dependent_name) similar to upper('check\\_[[:digit:]]+') escape '\\'
    ;

    ------ block 4 -----

    create or alter view v1 as select id,caption from test_table where id > 0;
    create or alter view v1 as select id from test_table where id > 0;
    commit;

    select upper(left(rdb$dependent_name,5)) dep_name4
    from rdb$database
    left join rdb$dependencies d on
        d.rdb$depended_on_name = upper( 'v1' )
        and trim(d.rdb$dependent_name) similar to upper('check\\_[[:digit:]]+') escape '\\'
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    DEP_NAME1                       <null>
    DEP_NAME2                       CHECK
    DEP_NAME2                       CHECK
    DEP_NAME2                       CHECK
    DEP_NAME3                       CHECK
    DEP_NAME3                       CHECK
    DEP_NAME3                       CHECK
    DEP_NAME4                       <null>
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

