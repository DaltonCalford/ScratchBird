#coding:utf-8

"""
ID:          issue-4151
ISSUE:       4151
TITLE:       Provide ability to return all columns using RETURNING (eg RETURNING *)
DESCRIPTION:
  Ability to use 'returning *' is verified both in DSL and PSQL.
  Checked on: 4.0.0.1455: OK, 1.337s.
NOTES:
[30.10.2019]
  NB: new datatype in FB 4.0 was introduces: numeric(38,0).
  It can lead to additional ident of values when we show them in form "SET LIST ON",
  so we have to ignore all internal spaces - see added 'substitution' section below.
JIRA:        CORE-3808
FBTEST:      bugs.core_3808
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test(id int default 2, x computed by ( id*2 ), y computed by ( x*x ), z computed by ( y*y ) );
    commit;

    insert into test default values returning *;

    update test set id=3 where id=2 returning *;

    set term ^;
    execute block returns( deleted_id int, deleted_x bigint, deleted_y bigint, deleted_z bigint ) as
    begin
        delete from test where id=3 returning * into deleted_id, deleted_x, deleted_y, deleted_z;
        suspend;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    ID                              2
    X                               4
    Y                               16
    Z                               256

    ID                              3
    X                               6
    Y                               36
    Z                               1296

    DELETED_ID                      3
    DELETED_X                       6
    DELETED_Y                       36
    DELETED_Z                       1296
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

