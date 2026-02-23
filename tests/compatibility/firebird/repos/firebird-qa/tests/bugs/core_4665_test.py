#coding:utf-8

"""
ID:          issue-1602
ISSUE:       1602
TITLE:       Wrong result when use "where <field_C> STARTING WITH <:value> ORDER BY <field_N>" and field_C is leading part of compound index key: { field_C, field_N }
DESCRIPTION:
JIRA:        CORE-4665
FBTEST:      bugs.core_4665
NOTES:
    [30.06.2025] pzotov
    Removed 'set plan on' because this test must check only result of query (data).

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test (id int, unit varchar(10), y int, z int);
    commit;
    delete from test;

    insert into test( id, unit, y, z) values (1, 'foo', 9999, 23636);
    insert into test( id, unit, y, z) values (2, 'foo', 8888, 22520);
    insert into test( id, unit, y, z) values (3, 'foo', 5555, 21822);
    insert into test( id, unit, y, z) values (4, 'foo', 3333, 17682);

    insert into test( id, unit, y, z) values (5, 'fooo', 1111, 22);
    insert into test( id, unit, y, z) values (6, 'fooo', 111, 222);
    insert into test( id, unit, y, z) values (7, 'fooo', 11, 2222);
    insert into test( id, unit, y, z) values (8, 'fooo', 1, 22222);
    commit;

    create index test_unit_y_asc on test( unit, y );
    commit;

    select 'point-1' msg, id, t.unit, t.y, t.z
    from test t
    where t.unit||'' starting with 'foo'
    order by t.y||'';

    select 'point-2' msg, id, t.unit, t.y, t.z
    from test t
    where t.unit starting with 'foo'
    order by t.y;

    commit;
    drop index test_unit_y_asc;
    commit;

    create descending index test_unit_y_desc on test( unit, y);
    commit;

    select 'point-3' msg, id, t.unit, t.y, t.z
    from test t
    where t.unit starting with 'foo'
    order by t.y;
"""

substitutions = [ ('[ \t]+', ' ') ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MSG                             point-1
    ID                              8
    UNIT                            fooo
    Y                               1
    Z                               22222
    MSG                             point-1
    ID                              7
    UNIT                            fooo
    Y                               11
    Z                               2222
    MSG                             point-1
    ID                              6
    UNIT                            fooo
    Y                               111
    Z                               222
    MSG                             point-1
    ID                              5
    UNIT                            fooo
    Y                               1111
    Z                               22
    MSG                             point-1
    ID                              4
    UNIT                            foo
    Y                               3333
    Z                               17682
    MSG                             point-1
    ID                              3
    UNIT                            foo
    Y                               5555
    Z                               21822
    MSG                             point-1
    ID                              2
    UNIT                            foo
    Y                               8888
    Z                               22520
    MSG                             point-1
    ID                              1
    UNIT                            foo
    Y                               9999
    Z                               23636
    MSG                             point-2
    ID                              8
    UNIT                            fooo
    Y                               1
    Z                               22222
    MSG                             point-2
    ID                              7
    UNIT                            fooo
    Y                               11
    Z                               2222
    MSG                             point-2
    ID                              6
    UNIT                            fooo
    Y                               111
    Z                               222
    MSG                             point-2
    ID                              5
    UNIT                            fooo
    Y                               1111
    Z                               22
    MSG                             point-2
    ID                              4
    UNIT                            foo
    Y                               3333
    Z                               17682
    MSG                             point-2
    ID                              3
    UNIT                            foo
    Y                               5555
    Z                               21822
    MSG                             point-2
    ID                              2
    UNIT                            foo
    Y                               8888
    Z                               22520
    MSG                             point-2
    ID                              1
    UNIT                            foo
    Y                               9999
    Z                               23636
    MSG                             point-3
    ID                              8
    UNIT                            fooo
    Y                               1
    Z                               22222
    MSG                             point-3
    ID                              7
    UNIT                            fooo
    Y                               11
    Z                               2222
    MSG                             point-3
    ID                              6
    UNIT                            fooo
    Y                               111
    Z                               222
    MSG                             point-3
    ID                              5
    UNIT                            fooo
    Y                               1111
    Z                               22
    MSG                             point-3
    ID                              4
    UNIT                            foo
    Y                               3333
    Z                               17682
    MSG                             point-3
    ID                              3
    UNIT                            foo
    Y                               5555
    Z                               21822
    MSG                             point-3
    ID                              2
    UNIT                            foo
    Y                               8888
    Z                               22520
    MSG                             point-3
    ID                              1
    UNIT                            foo
    Y                               9999
    Z                               23636
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

