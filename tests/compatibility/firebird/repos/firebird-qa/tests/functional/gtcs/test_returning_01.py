#coding:utf-8

"""
ID:          n/a
TITLE:       Test of RETURNING clause: INSERT, UPDATE, MERGE.
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_SQL_RETURNING_1.script
NOTES:
    [21.09.2025] pzotov
    Versions 3.x and 4.x can not pass this test:
        merge into ... using ... on ...
        when matched then ...
        ORDER BY ...
    -- fail with "Token unknown 'order' ..."
    See:
    https://github.com/FirebirdSQL/firebird/blob/6b73fa87cf667ad8f0839deaf8dd7c9473a34c32/doc/sql.extensions/README.returning#L63

    Checked on 6.0.0.1277; 5.0.4.1713.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """

    set list on;
    set term ^;

    create table tab (
      n1 integer primary key,
      n2 integer
    )^

    create table tab2 (
      n1 integer primary key,
      n2 integer
    )^

    insert into tab values (1, 10) returning n1, n2^
    insert into tab values (2, 20) returning n1, n2^

    execute block returns (o1 integer, o2 integer)
    as
    begin
      insert into tab values (3, 30) returning n1, n2 into o1, o2;
      suspend;
      insert into tab values (4, 40) returning n1, n2 into o1, o2;
      suspend;
    end^

    select 'point-000' as msg from rdb$database^
    select t.* from tab t order by n1^

    update or insert into tab values (3, 300) returning n1, n2, old.n1, old.n2, new.n1, new.n2^
    update or insert into tab values (5, 500) returning n1, n2, old.n1, old.n2, new.n1, new.n2^

    select 'point-050' as msg from rdb$database^
    select * from tab order by n1^

    execute block returns (o1 integer, o2 integer, o3 integer, o4 integer, o5 integer, o6 integer)
    as
    begin
      update or insert into tab values (2, 200) returning n1, n2, old.n1, old.n2, new.n1, new.n2
        into o1, o2, o3, o4, o5, o6;
      suspend;
      update or insert into tab values (6, 600) returning n1, n2, old.n1, old.n2, new.n1, new.n2
        into o1, o2, o3, o4, o5, o6;
      suspend;
    end^

    select 'point-100' as msg from rdb$database^
    select * from tab order by n1^

    commit^

    update tab set n2 = 1 returning n2^

    update tab set n2 = 2^
    update or insert into tab values (2, 2) matching (n2) returning n1, n2, old.n1, old.n2, new.n1, new.n2^

    select 'point-105' as msg from rdb$database^
    select * from tab order by n1^
    
    rollback^

    merge into tab2 using tab
      on tab2.n1 = tab.n1
      when matched then update set n1 = tab.n1, n2 = tab.n2
      order by tab2.n1, tab2.n2
      returning old.n1, old.n2, new.n1, new.n2, tab.n1, tab.n2^

    select 'point-107' as msg from rdb$database^
    select * from tab2 order by n1^

    merge into tab2 using tab
      on tab2.n1 = tab.n1
      when matched then update set n1 = tab.n1, n2 = tab.n2
      order by tab2.n1, tab2.n2
      returning n1, n2, old.n1, old.n2, new.n1, new.n2^

    select 'point-110' as msg from rdb$database^
    select * from tab2 order by n1^

    merge into tab2 using tab
      on tab2.n1 = tab.n1
      when matched then delete
      order by tab2.n1, tab2.n2
      returning n1, n2, old.n1, old.n2, new.n1, new.n2^

    select 'point-115' as msg from rdb$database^
    select * from tab2 order by n1^

    merge into tab2 using tab
      on tab2.n1 = tab.n1
      when not matched then insert values (n1, n2)
      order by tab2.n1, tab2.n2
      returning n1, n2, old.n1, old.n2, new.n1, new.n2^

    select 'point-120' as msg from rdb$database^
    select * from tab2^
    --

    merge into tab2 using (select n1 x1, n2 x2 from tab) tab
      on tab2.n1 = tab.x1
      when matched then update set n1 = tab.x1, n2 = tab.x2
      when not matched then insert values (x1, x2)
      order by tab2.n1, tab2.n2
      returning n1, n2, old.n1, old.n2, new.n1, new.n2^

    select 'point-122' as msg from rdb$database^
    select * from tab2 order by n1^

    merge into tab2 using (select n1 x1, n2 x2 from tab) tab
      on tab2.n1 = tab.x1
      when matched then update set n1 = tab.x1, n2 = tab.x2
      when not matched and 1 = 0 then insert values (x1, x2)
      order by tab2.n1, tab2.n2
      returning n1, n2, old.n1, old.n2, new.n1, new.n2^

    select 'point-125' as msg from rdb$database^
    select * from tab2 order by n1^

    merge into tab2 using (select n1 x1, n2 x2 from tab) tab
      on tab2.n1 = tab.x1
      when matched then update set n1 = tab.x1, n2 = tab.x2
      when not matched then insert values (x1, x2)^

    select 'point-130' as msg from rdb$database^
    select * from tab2^

    merge into tab2 using (select n1 x1, n2 x2 from tab) tab
      on tab2.n1 = tab.x1
      when matched and 1 = 0 then update set n1 = tab.x1, n2 = tab.x2
      when not matched then insert values (x1, x2)
      order by tab2.n1, tab2.n2
      returning n1, n2, old.n1, old.n2, new.n1, new.n2, x1, x2^

    select 'point-132' as msg from rdb$database^
    select * from tab2 order by n1^

    merge into tab2 using (select n1 x1, n2 x2 from tab where n1 = 1) tab
      on tab2.n1 = tab.x1
      when matched and 1 = 0 then update set n2 = tab.x2 * 10
      order by tab2.n1, tab2.n2
      returning n1, n2, old.n1, old.n2, new.n1, new.n2, x1, x2^

    select 'point-133' as msg from rdb$database^
    select * from tab2 order by n1^

    merge into tab2 using (select n1 x1, n2 x2 from tab where n1 = 1) tab
      on tab2.n1 = tab.x1
      when matched and 1 = 1 then update set n2 = tab.x2 * 10
      order by tab2.n1, tab2.n2
      returning n1, n2, old.n1, old.n2, new.n1, new.n2, x1, x2^

    select 'point-135' as msg from rdb$database^
    select * from tab2 order by n1^

    merge into tab2 using (select n1 x1, n2 x2 from tab where n1 = 4) tab
      on tab2.n1 = tab.x1
      when matched and 1 = 0 then delete
      order by tab2.n1, tab2.n2
      returning n1, n2, old.n1, old.n2, new.n1, new.n2, x1, x2^

    select 'point-137' as msg from rdb$database^
    select * from tab2 order by n1^

    merge into tab2 using (select n1 x1, n2 x2 from tab where n1 = 4) tab
      on tab2.n1 = tab.x1
      when matched and 1 = 1 then delete
      order by tab2.n1, tab2.n2
      returning n1, n2, old.n1, old.n2, new.n1, new.n2, x1, x2^

    select 'point-140' as msg from rdb$database^
    select * from tab2^
    --

    delete from tab^

    execute block returns (o1 integer, o2 integer, o3 integer, o4 integer, o5 integer, o6 integer)
    as
    begin
      merge into tab using tab2
        on tab.n1 = tab2.n1
        when matched and tab2.n2 = 100 then
          update set n1 = tab2.n1, n2 = tab.n2 + tab2.n2
        when not matched and tab2.n2 = 100 then
          insert values (n1, n2 * 10)
        returning tab2.n1, tab2.n2, old.n1, old.n2, new.n1, new.n2
        into o1, o2, o3, o4, o5, o6;
      suspend;

      merge into tab using tab2
        on tab.n1 = tab2.n1
        when matched and tab2.n2 = 100 then
          update set n1 = tab2.n1, n2 = tab.n2 + tab2.n2
        when not matched and tab2.n2 = 100 then
          insert values (n1, n2 * 10)
        returning tab2.n1, tab2.n2, old.n1, old.n2, new.n1, new.n2
        into o1, o2, o3, o4, o5, o6;
      suspend;

      merge into tab using tab2
        on tab.n1 = tab2.n1
        when not matched and tab2.n2 = 200 then
          insert values (n1, n2 * 10)
        returning tab2.n1, tab2.n2, old.n1, old.n2, new.n1, new.n2
        into o1, o2, o3, o4, o5, o6;
      suspend;

      merge into tab using tab2
        on tab.n1 = tab2.n1
        when matched and tab2.n2 = 200 then
          update set n1 = tab2.n1, n2 = tab.n2 + tab2.n2
        returning tab2.n1, tab2.n2, old.n1, old.n2, new.n1, new.n2
        into o1, o2, o3, o4, o5, o6;
      suspend;
    end^

    select 'point-150' as msg from rdb$database^
    select * from tab^

    ---

    merge into tab using tab2
      on tab.n1 = tab2.n1
      when not matched and tab2.n2 <= 200 then
        insert values (n1, n2 - 1)
      when not matched and tab2.n2 <= 500 then
        insert values (n1, n2 - 2)
      when not matched then
        insert values (n1, n2 - 3)
      when matched and tab.n2 < 1500 then
        update set n2 = tab2.n2 - 1
      when matched and tab.n2 < 2500 then
        delete^

    select 'point-160' as msg from rdb$database^
    select * from tab^

    delete from tab^

    merge into tab using (select * from tab2 where n1 = 3) tab2
      on tab.n1 = tab2.n1
      when not matched and tab2.n2 <= 200 then
        insert values (n1, n2 - 1)
      when not matched and tab2.n2 <= 500 then
        insert values (n1, n2 - 2)
      when not matched then
        insert values (n1, n2 - 3)
      when matched and tab.n2 < 1500 then
        update set n2 = tab2.n2 - 1
      when matched and tab.n2 < 2500 then
        delete
      order by tab.n1, tab.n2
      returning tab2.n2, tab.n2, old.n2, new.n2^

    select 'point-165' as msg from rdb$database^
    select * from tab^

    merge into tab using (select n1 x1, n2 x2 from tab2 where n1 = 3) tab2
      on n1 = x1
      when not matched and x2 <= 200 then
        insert values (x1, x2 - 1)
      when not matched and x2 <= 500 then
        insert values (x1, x2 - 2)
      when not matched then
        insert values (x1, x2 - 3)
      when matched and n2 < 1500 then
        update set n2 = x2 - 1
      when matched and n2 < 2500 then
        delete
      order by tab.n1, tab.n2
      returning x2, n2, old.n2, new.n2^

    select 'point-170' as msg from rdb$database^
    select * from tab^

    merge into tab using (select n1 x1, n2 x2 from tab2 where n1 = 3) tab2
      on n1 = x1
      when not matched and x2 <= 200 then
        insert values (x1, x2 - 1)
      when not matched and x2 <= 500 then
        insert values (x1, x2 - 2)
      when not matched then
        insert values (x1, x2 - 3)
      when matched and n2 < 2500 then
        delete
      order by tab.n1, tab.n2
      returning x2, n2, old.n2, new.n2^

    select 'point-175' as msg from rdb$database^
    select * from tab^

    merge into tab using (select n1 x1, n2 x2 from tab2 where n1 = 3) tab2
      on n1 = x1
      when not matched and x2 <= 200 then
        insert values (x1, x2 - 1)
      when not matched and x2 <= 500 then
        insert values (x1, x2 - 2)
      when not matched then
        insert values (x1, x2 - 3)
      when matched and n2 < 2500 then
        delete
      order by tab.n1, tab.n2
      returning x2, n2, old.n2, new.n2^

    select 'point-999' as msg from rdb$database^
    select * from tab^

    set term ;^

"""


# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions=[('=', ''), ('[ \t]+', ' ')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    expected_stdout = """
        N1 1
        N2 10
        N1 2
        N2 20
        O1 3
        O2 30
        O1 4
        O2 40
        MSG point-000
        N1 1
        N2 10
        N1 2
        N2 20
        N1 3
        N2 30
        N1 4
        N2 40
        N1 3
        N2 300
        CONSTANT 3
        CONSTANT 30
        N1 3
        N2 300
        N1 5
        N2 500
        CONSTANT <null>
        CONSTANT <null>
        N1 5
        N2 500
        MSG point-050
        N1 1
        N2 10
        N1 2
        N2 20
        N1 3
        N2 300
        N1 4
        N2 40
        N1 5
        N2 500
        O1 2
        O2 200
        O3 2
        O4 20
        O5 2
        O6 200
        O1 6
        O2 600
        O3 <null>
        O4 <null>
        O5 6
        O6 600
        MSG point-100
        N1 1
        N2 10
        N1 2
        N2 200
        N1 3
        N2 300
        N1 4
        N2 40
        N1 5
        N2 500
        N1 6
        N2 600
        N2 1
        N2 1
        N2 1
        N2 1
        N2 1
        N2 1
        Statement failed, SQLSTATE 23000
        violation of PRIMARY or UNIQUE KEY constraint "INTEG_2" on table "TAB"
        -Problematic key value is ("N1" 2)
        MSG point-105
        N1 1
        N2 2
        N1 2
        N2 2
        N1 3
        N2 2
        N1 4
        N2 2
        N1 5
        N2 2
        N1 6
        N2 2
        MSG point-107
        Statement failed, SQLSTATE 42702
        Dynamic SQL Error
        -SQL error code -204
        -Ambiguous field name between table TAB and table TAB2
        -N1
        MSG point-110
        Statement failed, SQLSTATE 42702
        Dynamic SQL Error
        -SQL error code -204
        -Ambiguous field name between table TAB and table TAB2
        -N1
        MSG point-115
        Statement failed, SQLSTATE 42702
        Dynamic SQL Error
        -SQL error code -204
        -Ambiguous field name between table TAB and table TAB2
        -N1
        MSG point-120
        N1 1
        N2 10
        N1 <null>
        N2 <null>
        N1 1
        N2 10
        N1 2
        N2 200
        N1 <null>
        N2 <null>
        N1 2
        N2 200
        N1 3
        N2 300
        N1 <null>
        N2 <null>
        N1 3
        N2 300
        N1 4
        N2 40
        N1 <null>
        N2 <null>
        N1 4
        N2 40
        N1 5
        N2 500
        N1 <null>
        N2 <null>
        N1 5
        N2 500
        N1 6
        N2 600
        N1 <null>
        N2 <null>
        N1 6
        N2 600
        MSG point-122
        N1 1
        N2 10
        N1 2
        N2 200
        N1 3
        N2 300
        N1 4
        N2 40
        N1 5
        N2 500
        N1 6
        N2 600
        N1 1
        N2 10
        N1 1
        N2 10
        N1 1
        N2 10
        N1 2
        N2 200
        N1 2
        N2 200
        N1 2
        N2 200
        N1 3
        N2 300
        N1 3
        N2 300
        N1 3
        N2 300
        N1 4
        N2 40
        N1 4
        N2 40
        N1 4
        N2 40
        N1 5
        N2 500
        N1 5
        N2 500
        N1 5
        N2 500
        N1 6
        N2 600
        N1 6
        N2 600
        N1 6
        N2 600
        MSG point-125
        N1 1
        N2 10
        N1 2
        N2 200
        N1 3
        N2 300
        N1 4
        N2 40
        N1 5
        N2 500
        N1 6
        N2 600
        MSG point-130
        N1 1
        N2 10
        N1 2
        N2 200
        N1 3
        N2 300
        N1 4
        N2 40
        N1 5
        N2 500
        N1 6
        N2 600
        MSG point-132
        N1 1
        N2 10
        N1 2
        N2 200
        N1 3
        N2 300
        N1 4
        N2 40
        N1 5
        N2 500
        N1 6
        N2 600
        MSG point-133
        N1 1
        N2 10
        N1 2
        N2 200
        N1 3
        N2 300
        N1 4
        N2 40
        N1 5
        N2 500
        N1 6
        N2 600
        N1 1
        N2 100
        N1 1
        N2 10
        N1 1
        N2 100
        X1 1
        X2 10
        MSG point-135
        N1 1
        N2 100
        N1 2
        N2 200
        N1 3
        N2 300
        N1 4
        N2 40
        N1 5
        N2 500
        N1 6
        N2 600
        MSG point-137
        N1 1
        N2 100
        N1 2
        N2 200
        N1 3
        N2 300
        N1 4
        N2 40
        N1 5
        N2 500
        N1 6
        N2 600
        N1 4
        N2 40
        N1 4
        N2 40
        CONSTANT <null>
        CONSTANT <null>
        X1 4
        X2 40
        MSG point-140
        N1 1
        N2 100
        N1 2
        N2 200
        N1 3
        N2 300
        N1 5
        N2 500
        N1 6
        N2 600
        O1 1
        O2 100
        O3 <null>
        O4 <null>
        O5 1
        O6 1000
        O1 1
        O2 100
        O3 1
        O4 1000
        O5 1
        O6 1100
        O1 2
        O2 200
        O3 <null>
        O4 <null>
        O5 2
        O6 2000
        O1 2
        O2 200
        O3 2
        O4 2000
        O5 2
        O6 2200
        MSG point-150
        N1 1
        N2 1100
        N1 2
        N2 2200
        MSG point-160
        N1 1
        N2 99
        N1 3
        N2 298
        N1 5
        N2 498
        N1 6
        N2 597
        N2 300
        N2 298
        N2 <null>
        N2 298
        MSG point-165
        N1 3
        N2 298
        X2 300
        N2 299
        N2 298
        N2 299
        MSG point-170
        N1 3
        N2 299
        X2 300
        N2 299
        N2 299
        CONSTANT <null>
        MSG point-175
        X2 300
        N2 298
        N2 <null>
        CONSTANT 298
        MSG point-999
        N1 3
        N2 298
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
